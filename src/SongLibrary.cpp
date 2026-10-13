#include "SongLibrary.h"
#include "SongFiles.h"
#include "sqlite3/sqlite3raii.hpp"
#include <algorithm>
#include <array>
#include <cereal/archives/json.hpp>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include <xxhash.h>

namespace songs {
namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
constexpr int schemaVersion = 1;
constexpr size_t hashBufferBytes = 64 * 1024;
constexpr size_t persistenceBatchSize = 32;
constexpr auto publicationInterval = std::chrono::milliseconds(250);
constexpr double millisecondsToSeconds = .001;

std::string trim(const std::string &value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

struct Signature {
    int64_t size = 0;
    std::string modified;
    bool operator==(const Signature &other) const {
        return size == other.size && modified == other.modified;
    }
};

Signature signature(const fs::path &path) {
    const auto size = fs::file_size(path);
    if (size > static_cast<uintmax_t>(std::numeric_limits<int64_t>::max())) {
        throw std::runtime_error("File too large: " + path.u8string());
    }
    return {
        static_cast<int64_t>(size),
        std::to_string(fs::last_write_time(path).time_since_epoch().count())};
}

struct FileRow {
    std::string path, song, role, hash, payload, dependency, error;
    Signature signature;
    std::optional<double> duration;
};

std::string hashFile(const fs::path &file, const std::atomic<bool> &cancel) {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot read " + file.u8string());
    }
    XXH3_state_t state;
    XXH3_128bits_reset(&state);
    std::array<char, hashBufferBytes> buffer{};
    while (input && !cancel.load()) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        XXH3_128bits_update(&state, buffer.data(),
                            static_cast<size_t>(input.gcount()));
    }
    if (cancel.load()) {
        throw std::runtime_error("Scan cancelled");
    }
    if (!input.eof()) {
        throw std::runtime_error("Cannot read complete file: " +
                                 file.u8string());
    }
    XXH128_canonical_t canonical;
    XXH128_canonicalFromHash(&canonical, XXH3_128bits_digest(&state));
    std::ostringstream out;
    for (const auto byte : canonical.digest) {
        out << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(byte);
    }
    return out.str();
}

template <class T> std::string encode(const T &value) {
    std::ostringstream out;
    {
        cereal::JSONOutputArchive archive(out);
        archive(value);
    }
    return out.str();
}

template <class T> T decode(const std::string &text) {
    T value;
    std::istringstream input(text);
    cereal::JSONInputArchive archive(input);
    archive(value);
    return value;
}

struct ChartCache {
    std::string name, artist, charter, error;
    std::map<std::string, std::string> fields;
    std::vector<std::string> parts, tracks, identification;
    double end = 0, initial = 0, minimum = 0, maximum = 0;
    template <class Archive> void serialize(Archive &a) {
        a(name, artist, charter, error, fields, parts, tracks, identification,
          end, initial, minimum, maximum);
    }
};

ChartCache summarize(const CChart::Inspection &input) {
    ChartCache result;
    result.name = input.metadata.name;
    result.artist = input.metadata.artist;
    result.charter = input.metadata.charter;
    result.error = input.error;
    result.fields = input.metadata.fields;
    result.parts = input.instruments;
    result.tracks = input.trackNames;
    result.identification = input.identification;
    result.end = input.lastNoteEnd;
    for (const auto &bpm : input.bpm) {
        if (std::isfinite(bpm.lTime) && bpm.lTime > 0) {
            if (result.initial <= 0) {
                result.initial = result.minimum = result.maximum = bpm.lTime;
            }
            result.minimum = std::min(result.minimum, bpm.lTime);
            result.maximum = std::max(result.maximum, bpm.lTime);
        }
    }
    return result;
}

std::map<std::string, std::string> readIni(const fs::path &file) {
    std::ifstream input(file);
    if (!input) {
        throw std::runtime_error("Cannot read " + file.u8string());
    }
    std::map<std::string, std::string> result;
    bool song = false;
    std::string line;
    while (std::getline(input, line)) {
        if (line.compare(0, 3, "\xef\xbb\xbf") == 0) {
            line.erase(0, 3);
        }
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }
        if (line.front() == '[') {
            song = songfiles::lower(line) == "[song]";
        } else if (song) {
            const auto equals = line.find('=');
            if (equals != std::string::npos) {
                result[songfiles::lower(trim(line.substr(0, equals)))] =
                    CChart::unquoteSongMetadata(line.substr(equals + 1));
            }
        }
    }
    if (!input.eof()) {
        throw std::runtime_error("Incomplete song.ini");
    }
    return result;
}

std::optional<double> number(const std::string &value, double scale = 1) {
    try {
        size_t end = 0;
        const double result = std::stod(value, &end) * scale;
        if (end == value.size() && std::isfinite(result) && result >= 0) {
            return result;
        }
    } catch (const std::exception &) {
        return {}; // Invalid optional metadata is not a scan failure.
    }
    return {};
}

bool usefulTrackTitle(const std::string &name) {
    const auto lower = songfiles::lower(trim(name));
    return !lower.empty() && lower != "notes" && lower != "song" &&
           lower != "tempo track" && lower != "tempo" && lower != "events" &&
           lower != "beat" && lower != "venue" && lower != "t1 gems" &&
           lower.compare(0, std::string_view("part ").size(), "part ") != 0;
}

std::string column(sqlite3_stmt *stmt, int index) {
    const auto *value = sqlite3_column_text(stmt, index);
    return value != nullptr
               ? std::string(
                     reinterpret_cast<const char *>(value),
                     static_cast<size_t>(sqlite3_column_bytes(stmt, index)))
               : std::string{};
}

class Store {
  public:
    std::unique_ptr<SqliteDb> db;
    std::map<std::string, SongRecord> songs;
    std::map<std::string, std::map<std::string, FileRow>> files;
    std::string warning;

    void open(const fs::path &path) {
        try {
            if (!path.parent_path().empty()) {
                fs::create_directories(path.parent_path());
            }
            db = std::make_unique<SqliteDb>(path.u8string());
            db->exec("PRAGMA foreign_keys=ON; PRAGMA busy_timeout=250;");
            int version = 0;
            {
                auto statement =
                    db->prepare("PRAGMA user_version", "index version");
                if (statement.step() == SQLITE_ROW) {
                    version = sqlite3_column_int(statement.get(), 0);
                }
            }
            if (version != schemaVersion) {
                db->exec("BEGIN; DROP TABLE IF EXISTS files; DROP TABLE IF "
                         "EXISTS songs; "
                         "DROP TABLE IF EXISTS library_meta; COMMIT;");
            }
            db->exec(
                "CREATE TABLE IF NOT EXISTS songs (path TEXT PRIMARY KEY, "
                "title TEXT, "
                "artist TEXT, charter TEXT, record TEXT NOT NULL);"
                "CREATE TABLE IF NOT EXISTS files (path TEXT PRIMARY KEY, song "
                "TEXT NOT NULL "
                "REFERENCES songs(path) ON DELETE CASCADE, role TEXT, size "
                "INTEGER, modified TEXT, "
                "hash TEXT, payload TEXT, dependency TEXT, duration REAL, "
                "error TEXT);"
                "CREATE INDEX IF NOT EXISTS songs_title ON songs(title);"
                "CREATE INDEX IF NOT EXISTS songs_artist ON songs(artist);"
                "CREATE INDEX IF NOT EXISTS songs_charter ON songs(charter);"
                "CREATE TABLE IF NOT EXISTS library_meta (fingerprint TEXT);"
                "PRAGMA user_version=1;");
            // file_time_type's epoch/period is implementation-defined. Its ABI
            // fingerprint prevents silently trusting timestamps from another
            // build.
            const std::string fingerprint =
                "extractor-1:xxh3-128:" +
                std::to_string(fs::file_time_type::period::num) + ":" +
                std::to_string(fs::file_time_type::period::den) + ":" +
#ifdef _WIN32
                "windows";
#else
                "unix";
#endif
#if defined(_LIBCPP_VERSION)
            const auto runtimeFingerprint =
                fingerprint + ":libc++:" + std::to_string(_LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
            const auto runtimeFingerprint =
                fingerprint + ":libstdc++:" + std::to_string(__GLIBCXX__);
#else
                const auto runtimeFingerprint = fingerprint;
#endif
            bool matches = false;
            {
                auto query = db->prepare("SELECT fingerprint FROM library_meta",
                                         "extractor version");
                matches = query.step() == SQLITE_ROW &&
                          column(query.get(), 0) == runtimeFingerprint;
            }
            if (!matches) {
                SqliteTxn transaction(*db);
                db->exec("DELETE FROM songs; DELETE FROM library_meta;");
                auto insert = db->prepare("INSERT INTO library_meta VALUES (?)",
                                          "extractor version");
                (void)insert.bind(1, runtimeFingerprint).step();
                transaction.commit();
            }
            auto query = db->prepare("SELECT record FROM songs ORDER BY path",
                                     "cached songs");
            while (query.step() == SQLITE_ROW) {
                auto record = decode<SongRecord>(column(query.get(), 0));
                songs.emplace(record.path, std::move(record));
            }
            auto fileQuery =
                db->prepare("SELECT "
                            "path,song,role,size,modified,hash,payload,"
                            "dependency,duration,error FROM files",
                            "cached files");
            while (fileQuery.step() == SQLITE_ROW) {
                auto *stmt = fileQuery.get();
                FileRow row;
                row.path = column(stmt, 0);
                row.song = column(stmt, 1);
                row.role = column(stmt, 2);
                row.signature = {sqlite3_column_int64(stmt, 3),
                                 column(stmt, 4)};
                row.hash = column(stmt, 5);
                row.payload = column(stmt, 6);
                row.dependency = column(stmt, 7);
                if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
                    row.duration = sqlite3_column_double(stmt, 8);
                }
                row.error = column(stmt, 9);
                files[row.song].emplace(row.path, std::move(row));
            }
        } catch (const SqliteError &error) {
            const int primary = error.code & 255;
            db.reset();
            if (primary == SQLITE_CORRUPT || primary == SQLITE_NOTADB) {
                // Preserve the broken cache for diagnosis; never touch
                // songs/saves.
                std::error_code ec;
                auto backup = path;
                backup +=
                    ".invalid-" +
                    std::to_string(Clock::now().time_since_epoch().count());
                fs::rename(path, backup, ec);
                if (!ec) {
                    songs.clear();
                    files.clear();
                    open(path);
                    warning = "Indice danificado reconstruido";
                    return;
                }
            }
            warning = "Cache indisponivel; usando memoria: " +
                      std::string(error.what());
        } catch (const std::exception &error) {
            // A malformed serialized record is derived data, too. Drop only
            // index rows, then rebuild; keep a usable DB when possible.
            songs.clear();
            files.clear();
            if (db) {
                try {
                    db->exec("DELETE FROM songs;");
                } catch (const std::exception &) {
                    db.reset();
                }
            }
            warning = "Indice sera reconstruido: " + std::string(error.what());
        }
    }

    void persist(const std::set<std::string> &paths) {
        if (!db || paths.empty()) {
            return;
        }
        try {
            SqliteTxn transaction(*db);
            auto remove =
                db->prepare("DELETE FROM songs WHERE path=?", "replace song");
            auto insert = db->prepare("INSERT INTO songs VALUES(?,?,?,?,?)",
                                      "cache song");
            auto file = db->prepare(
                "INSERT INTO files VALUES(?,?,?,?,?,?,?,?,?,?)", "cache file");
            for (const auto &path : paths) {
                (void)remove.bind(1, path).step();
                remove.reset();
                const auto song = songs.find(path);
                if (song == songs.end()) {
                    continue;
                }
                const auto &record = song->second;
                (void)insert.bind(1, path)
                    .bind(2, record.title)
                    .bind(3, record.artist)
                    .bind(4, record.charter)
                    .bind(5, encode(record))
                    .step();
                insert.reset();
                for (const auto &entry : files.at(path)) {
                    const auto &row = entry.second;
                    file.bind(1, row.path)
                        .bind(2, path)
                        .bind(3, row.role)
                        .bind(4, row.signature.size)
                        .bind(5, row.signature.modified)
                        .bind(6, row.hash)
                        .bind(7, row.payload)
                        .bind(8, row.dependency);
                    if (row.duration) {
                        file.bind(9, *row.duration);
                    } else {
                        check_sqlite_rc(db->get(),
                                        sqlite3_bind_null(file.get(), 9),
                                        "audio duration");
                    }
                    (void)file.bind(10, row.error).step();
                    file.reset();
                }
            }
            transaction.commit();
        } catch (const std::exception &error) {
            db.reset();
            warning = "Cache indisponivel; usando memoria: " +
                      std::string(error.what());
        }
    }
};
} // namespace

struct SongLibrary::State {
    Store store;
};

SongLibrary::SongLibrary(fs::path root, fs::path database, Inspector inspector,
                         AudioProbe audio)
    : root_(std::move(root)), database_(std::move(database)),
      inspector_(inspector ? std::move(inspector)
                           : Inspector([](const fs::path &file) {
                                 return CChart::inspect(file.u8string());
                             })),
      audio_(audio ? std::move(audio) : AudioProbe(probeAudio)),
      published_(std::make_shared<const LibrarySnapshot>()) {}

SongLibrary::~SongLibrary() {
    cancel();
    wait();
}

std::shared_ptr<const LibrarySnapshot> SongLibrary::snapshot() const {
    return std::atomic_load(&published_);
}

bool SongLibrary::running() const noexcept {
    std::lock_guard<std::mutex> lock(requestMutex_);
    return active_;
}

void SongLibrary::refresh(ScanMode mode) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    if (active_) {
        if (!pending_ || mode == ScanMode::Full) {
            pending_ = mode;
        }
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    cancel_ = false;
    active_ = true;
    try {
        worker_ = std::thread(&SongLibrary::run, this, mode);
    } catch (...) {
        active_ = false;
        throw;
    }
}

void SongLibrary::cancel() noexcept {
    cancel_ = true;
    std::lock_guard<std::mutex> lock(requestMutex_);
    pending_.reset();
}

void SongLibrary::wait() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void SongLibrary::run(ScanMode mode) noexcept {
    // Sequential workers retain the in-memory cache when persistence fails.
    // Only this worker accesses state_; the owner joins before replacement.
    ScanProgress progress;
    try {
        if (!state_) {
            state_ = std::make_unique<State>();
            state_->store.open(database_);
        }
    } catch (...) {
        std::lock_guard<std::mutex> lock(requestMutex_);
        active_ = false;
        return;
    }
    auto &store = state_->store;
    const auto previousSnapshot = snapshot();
    uint64_t revision = previousSnapshot->revision;
    uint64_t contentRevision = previousSnapshot->contentRevision;
    bool contentChanged =
        previousSnapshot->songs.empty() && !store.songs.empty();
    auto lastPublish = Clock::now();
    std::set<std::string> dirty;
    auto publish = [&](bool force) {
        if (!force && Clock::now() - lastPublish < publicationInterval) {
            return;
        }
        store.persist(dirty);
        dirty.clear();
        auto next = std::make_shared<LibrarySnapshot>();
        next->revision = ++revision;
        if (contentChanged) {
            ++contentRevision;
            contentChanged = false;
        }
        next->contentRevision = contentRevision;
        next->progress = progress;
        if (!store.warning.empty()) {
            next->progress.warning =
                store.warning +
                (progress.warning.empty() ? "" : "; " + progress.warning);
        }
        for (const auto &entry : store.songs) {
            next->songs.push_back(entry.second);
        }
        std::shared_ptr<const LibrarySnapshot> immutable = std::move(next);
        std::atomic_store(&published_, std::move(immutable));
        lastPublish = Clock::now();
    };
    try {
        progress.running = true;
        progress.mode = mode;
        publish(true); // Persisted records are available before any filesystem
                       // scan.
        for (;;) {
            progress = {};
            progress.running = true;
            progress.mode = mode;
            bool complete = true;
            std::set<std::string> seen;
            std::vector<fs::path> directories;
            try {
                if (!fs::is_directory(root_)) {
                    throw std::runtime_error("Pasta de musicas indisponivel");
                }
                directories.push_back(root_);
                for (const auto &entry :
                     fs::recursive_directory_iterator(root_)) {
                    if (cancel_) {
                        break;
                    }
                    if (entry.is_directory() && !entry.is_symlink()) {
                        directories.push_back(entry.path());
                    }
                    publish(false);
                }
            } catch (const std::exception &error) {
                complete = false;
                progress.warning = error.what();
            }
            for (const auto &directory : directories) {
                if (cancel_) {
                    break;
                }
                const auto relative =
                    directory.lexically_relative(root_).generic_u8string();
                try {
                    const auto entries = songfiles::list(directory);
                    const auto candidates =
                        songfiles::charts(directory, entries);
                    if (candidates.empty()) {
                        continue;
                    }
                    seen.insert(relative);
                    const auto ini = songfiles::named(entries, "song.ini");
                    const auto mainAudio = songfiles::audio(directory, entries);
                    fs::path cover;
                    for (const auto *name :
                         {"album.png", "album.jpg", "album.jpeg", "cover.png",
                          "cover.jpg"}) {
                        cover = songfiles::named(entries, name);
                        if (!cover.empty()) {
                            break;
                        }
                    }
                    std::map<std::string, FileRow> current;
                    const auto &previousFiles = store.files[relative];
                    bool changed = mode == ScanMode::Full ||
                                   store.songs.count(relative) == 0;
                    for (const auto &path : entries) {
                        const auto extension =
                            songfiles::lower(path.extension().string());
                        const bool chart =
                            std::find(candidates.begin(), candidates.end(),
                                      path) != candidates.end();
                        const bool audio =
                            extension == ".ogg" || extension == ".mp3";
                        if (!chart && path != ini && !audio && path != cover) {
                            continue;
                        }
                        if (cancel_) {
                            break;
                        }
                        ++progress.examined;
                        FileRow row;
                        row.path =
                            path.lexically_relative(root_).generic_u8string();
                        row.song = relative;
                        if (chart) {
                            row.role = "chart";
                        } else if (path == ini) {
                            row.role = "ini";
                        } else {
                            row.role = audio ? "audio" : "cover";
                        }
                        row.signature = signature(path);
                        const auto old = previousFiles.find(row.path);
                        const bool same =
                            old != previousFiles.end() &&
                            row.signature == old->second.signature &&
                            row.role == old->second.role;
                        if (same) {
                            row = old->second;
                        } else {
                            changed = true;
                        }
                        if ((chart || path == ini) &&
                            (!same || mode == ScanMode::Full)) {
                            row.hash = hashFile(path, cancel_);
                            ++progress.hashes;
                            if (old != previousFiles.end() &&
                                row.hash == old->second.hash) {
                                row.payload = old->second.payload;
                                row.dependency = old->second.dependency;
                            }
                        }
                        current.emplace(row.path, std::move(row));
                    }
                    changed = changed || previousFiles.size() != current.size();
                    if (cancel_) {
                        break;
                    }
                    if (!changed) {
                        ++progress.reused;
                        publish(false);
                        continue;
                    }
                    SongRecord record;
                    record.path = relative;
                    const auto key = [&](const fs::path &path) {
                        return path.lexically_relative(root_)
                            .generic_u8string();
                    };
                    std::string iniHash;
                    if (!ini.empty()) {
                        auto &row = current.at(key(ini));
                        iniHash = row.hash;
                        if (row.payload.empty() || mode == ScanMode::Full) {
                            record.iniFields = readIni(ini);
                            row.payload = encode(record.iniFields);
                        } else {
                            record.iniFields =
                                decode<std::map<std::string, std::string>>(
                                    row.payload);
                        }
                    }
                    ChartCache chart;
                    for (const auto &path : candidates) {
                        if (cancel_) {
                            break;
                        }
                        auto &row = current.at(key(path));
                        if (row.payload.empty() || row.dependency != iniHash ||
                            mode == ScanMode::Full) {
                            chart = summarize(inspector_(path));
                            ++progress.chartParses;
                            row.payload = encode(chart);
                            row.dependency = iniHash;
                        } else {
                            chart = decode<ChartCache>(row.payload);
                        }
                        if (!chart.parts.empty()) {
                            record.chartPath = row.path;
                            record.format =
                                songfiles::lower(path.extension().string());
                            break;
                        }
                    }
                    record.chartFields = chart.fields;
                    record.instruments = chart.parts;
                    record.trackNames = chart.tracks;
                    record.identification = chart.identification;
                    record.chartEnd = chart.end;
                    record.bpmInitial = chart.initial;
                    record.bpmMin = chart.minimum;
                    record.bpmMax = chart.maximum;
                    std::map<std::string, std::string> embedded;
                    for (const auto &field : chart.fields) {
                        embedded[songfiles::lower(field.first)] = field.second;
                    }
                    if (!chart.name.empty()) {
                        embedded["name"] = chart.name;
                    }
                    if (!chart.artist.empty()) {
                        embedded["artist"] = chart.artist;
                    }
                    if (!chart.charter.empty()) {
                        embedded["charter"] = chart.charter;
                    }
                    // Explicit key=value identification text is accepted; free
                    // text and copyright notices are retained without guessing
                    // authors.
                    for (const auto &text : chart.identification) {
                        const auto equals = text.find('=');
                        if (equals != std::string::npos) {
                            embedded[songfiles::lower(
                                trim(text.substr(0, equals)))] =
                                CChart::unquoteSongMetadata(
                                    text.substr(equals + 1));
                        }
                    }
                    const auto field = [&](const std::string &name) {
                        const auto internal = embedded.find(name);
                        if (internal != embedded.end() &&
                            !trim(internal->second).empty()) {
                            record.sources[name] = "chart";
                            return internal->second;
                        }
                        const auto external = record.iniFields.find(name);
                        if (external != record.iniFields.end() &&
                            !trim(external->second).empty()) {
                            record.sources[name] = "song.ini";
                            return external->second;
                        }
                        return std::string{};
                    };
                    record.title = field("name");
                    if (record.title.empty() && !chart.tracks.empty() &&
                        usefulTrackTitle(chart.tracks.front())) {
                        record.title = chart.tracks.front();
                        record.sources["name"] = "midi-sequence";
                    }
                    if (record.title.empty()) {
                        record.title = directory.filename().u8string();
                        record.sources["name"] = "directory";
                    }
                    record.artist = field("artist");
                    record.charter = field("charter");
                    if (record.charter.empty()) {
                        record.charter = field("frets");
                    }
                    record.album = field("album");
                    record.genre = field("genre");
                    record.year = field("year");
                    record.origin = field("icon");
                    record.previewStart = number(field("previewstart"));
                    if (!record.previewStart) {
                        record.previewStart = number(
                            field("preview_start_time"), millisecondsToSeconds);
                    }
                    record.previewEnd = number(field("previewend"));
                    record.coverPath = cover.empty() ? "" : key(cover);
                    if (!mainAudio.empty()) {
                        record.audioPath = key(mainAudio);
                        auto &row = current.at(record.audioPath);
                        const auto old = previousFiles.find(row.path);
                        if (mode == ScanMode::Full ||
                            old == previousFiles.end() ||
                            !(row.signature == old->second.signature) ||
                            (!row.duration && row.error.empty())) {
                            const auto info = audio_(mainAudio);
                            row.duration = info.duration;
                            row.error = info.error;
                            ++progress.audioProbes;
                        }
                        record.duration = row.duration;
                        if (record.duration) {
                            record.durationSource = "audio";
                        }
                        record.error = row.error;
                    }
                    if (!record.duration) {
                        record.duration =
                            number(field("song_length"), millisecondsToSeconds);
                        if (record.duration && *record.duration > 0) {
                            record.durationSource = "metadata";
                        } else if (record.chartEnd > 0) {
                            record.duration = record.chartEnd;
                            record.durationSource = "chart-estimate";
                        } else {
                            record.duration.reset();
                        }
                    }
                    for (const auto *stem :
                         {"guitar", "bass", "rhythm", "keys", "drums",
                          "drums_1", "drums_2", "drums_3", "drums_4", "vocals",
                          "vocals_1", "vocals_2", "crowd"}) {
                        for (const auto *extension : {".ogg", ".mp3"}) {
                            if (!songfiles::named(entries,
                                                  std::string(stem) + extension)
                                     .empty()) {
                                record.stems.emplace_back(stem);
                                break;
                            }
                        }
                    }
                    record.available = !record.chartPath.empty() &&
                                       !record.audioPath.empty() &&
                                       record.error.empty();
                    if (record.chartPath.empty()) {
                        record.error = chart.error.empty()
                                           ? "Chart indisponivel"
                                           : chart.error;
                    } else if (record.audioPath.empty()) {
                        record.error = "Audio principal ausente";
                    }
                    // Commit signatures only if every inspected dependency is
                    // still the same file version; additions are caught next
                    // scan.
                    for (const auto &entry : current) {
                        if (!(signature(root_ / fs::u8path(entry.first)) ==
                              entry.second.signature)) {
                            throw std::runtime_error(
                                "Arquivo alterado durante a leitura: " +
                                entry.first);
                        }
                    }
                    if (cancel_) {
                        break;
                    }
                    if (store.songs.count(relative) != 0) {
                        ++progress.modified;
                    } else {
                        ++progress.added;
                    }
                    contentChanged = true;
                    store.songs[relative] = std::move(record);
                    store.files[relative] = std::move(current);
                    dirty.insert(relative);
                    if (dirty.size() >= persistenceBatchSize) {
                        store.persist(dirty);
                        dirty.clear();
                    }
                    publish(false);
                } catch (const std::exception &error) {
                    complete = false;
                    if (!cancel_) {
                        progress.warning = error.what();
                    }
                }
            }
            if (complete && !cancel_) {
                for (auto it = store.songs.begin(); it != store.songs.end();) {
                    if (seen.count(it->first) == 0) {
                        dirty.insert(it->first);
                        it = store.songs.erase(it);
                        ++progress.removed;
                        contentChanged = true;
                    } else {
                        ++it;
                    }
                }
                for (auto it = store.files.begin(); it != store.files.end();) {
                    if (store.songs.count(it->first) == 0) {
                        it = store.files.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            progress.running = false;
            progress.cancelled = cancel_;
            progress.complete = complete && !cancel_;
            publish(true);
            std::lock_guard<std::mutex> lock(requestMutex_);
            if (!cancel_ && pending_) {
                mode = *pending_;
                pending_.reset();
            } else {
                active_ = false;
                return;
            }
        }
    } catch (const std::exception &error) {
        progress.running = false;
        progress.warning = error.what();
        try {
            publish(true);
        } catch (...) {
            // Keep the last immutable snapshot if publishing also fails.
        }
    } catch (...) {
        // Destruction must still be able to join after allocation failures.
    }
    std::lock_guard<std::mutex> lock(requestMutex_);
    active_ = false;
}
} // namespace songs
