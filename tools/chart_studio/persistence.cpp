#include "core.h"
#include "sqlite3/sqlite3raii.hpp"
#include <chrono>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace ChartStudio {
namespace {
constexpr int application = 1196442451;
std::string column(sqlite3_stmt *stmt, int index) {
    const auto *data = sqlite3_column_text(stmt, index);
    return data ? std::string(reinterpret_cast<const char *>(data),
                              size_t(sqlite3_column_bytes(stmt, index)))
                : std::string();
}
void schema(SqliteDb &db, const std::string &kind, bool create) {
    auto version = db.prepare("PRAGMA user_version", "schema version");
    (void)version.step();
    const int found = sqlite3_column_int(version.get(), 0);
    auto app = db.prepare("PRAGMA application_id", "application identity");
    (void)app.step();
    if (create && found == 0 && sqlite3_column_int(app.get(), 0) == 0) {
        SqliteTxn tx(db);
        db.exec(
            "CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
            "CREATE TABLE patterns(exact TEXT PRIMARY KEY, structural TEXT NOT "
            "NULL, payload TEXT NOT NULL, status INTEGER NOT NULL CHECK(status "
            "BETWEEN 0 AND 2));"
            "CREATE INDEX pattern_structural ON patterns(structural);"
            "PRAGMA application_id=1196442451; PRAGMA user_version=1;");
        auto insert = db.prepare("INSERT INTO metadata VALUES('kind',?)",
                                 "database kind");
        (void)insert.bind(1, kind).step();
        tx.commit();
    } else if (found != 1 || sqlite3_column_int(app.get(), 0) != application) {
        throw std::runtime_error("Unknown or incompatible Chart Studio "
                                 "database; original file was preserved");
    }
    auto query = db.prepare("SELECT value FROM metadata WHERE key='kind'",
                            "database kind");
    if (query.step() != SQLITE_ROW || column(query.get(), 0) != kind) {
        throw std::runtime_error("Expected Chart Studio " + kind + " database");
    }
}
void replaceFile(const fs::path &temporary, const fs::path &destination,
                 bool replace) {
    if (fs::exists(destination) && !replace) {
        throw std::runtime_error(
            "Destination exists; explicitly allow replacement: " +
            destination.u8string());
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_WRITE_THROUGH |
                         (replace ? MOVEFILE_REPLACE_EXISTING : 0))) {
        throw std::runtime_error("Cannot replace destination file");
    }
#else
    if (!replace) {
        // A hard link publishes without a check/rename overwrite race.
        fs::create_hard_link(temporary, destination);
        fs::remove(temporary);
    } else {
        fs::rename(temporary, destination);
    }
#endif
}
struct Temporary {
    fs::path directory, file;
    explicit Temporary(const fs::path &destination) {
        auto parent = fs::absolute(destination).parent_path();
        fs::create_directories(parent);
        for (unsigned i = 0; i < 100; ++i) {
            directory =
                parent / (".chart-studio-" +
                          std::to_string(std::chrono::steady_clock::now()
                                             .time_since_epoch()
                                             .count()) +
                          "-" + std::to_string(i));
            if (fs::create_directory(directory)) {
                file = directory / "output";
                return;
            }
        }
        throw std::runtime_error("Cannot reserve output temporary directory");
    }
    ~Temporary() {
        std::error_code error;
        fs::remove_all(directory, error);
    }
};
std::string patternText(const Pattern &pattern) {
    std::ostringstream out;
    {
        cereal::JSONOutputArchive archive(out);
        archive(pattern);
    }
    return out.str();
}
void insertPattern(SqliteDb &db, const Pattern &pattern) {
    auto insert = db.prepare("INSERT OR REPLACE INTO patterns VALUES(?,?,?,?)",
                             "write pattern");
    (void)insert.bind(1, pattern.exact)
        .bind(2, pattern.structural)
        .bind(3, patternText(pattern))
        .bind(4, pattern.status)
        .step();
}
void rebase(State &state, const fs::path &directory, bool saving) {
    for (auto *name : {&state.input, &state.audio, &state.reference}) {
        if (name->empty()) {
            continue;
        }
        auto path = fs::u8path(*name);
        if (saving) {
            std::error_code error;
            const auto relative = fs::relative(path, directory, error);
            if (!error && !relative.empty()) {
                *name = relative.generic_u8string();
            }
        } else if (path.is_relative()) {
            *name = (directory / path).lexically_normal().u8string();
        }
    }
}
} // namespace

void protectDestination(const fs::path &path,
                        const std::vector<fs::path> &inputs) {
    if (path.empty()) {
        throw std::runtime_error("Empty destination");
    }
    for (const auto &input : inputs) {
        if (input.empty()) {
            continue;
        }
        std::error_code error;
        if (fs::weakly_canonical(fs::absolute(path)) ==
                fs::weakly_canonical(fs::absolute(input)) ||
            (fs::exists(path) && fs::equivalent(path, input, error))) {
            throw std::runtime_error(
                "Destination conflicts with another source/project/output: " +
                path.u8string());
        }
    }
}

void safeWrite(const fs::path &path, const std::string &text, bool replace) {
    Temporary temp(path);
    {
        std::ofstream output(temp.file, std::ios::binary);
        output.write(text.data(), std::streamsize(text.size()));
        output.flush();
        if (!output) {
            throw std::runtime_error("Cannot finish writing output");
        }
    }
    replaceFile(temp.file, path, replace);
}

void saveProject(const fs::path &path, const State &state, bool replace) {
    validate(state);
    Temporary temp(path);
    auto saved = state;
    rebase(saved, fs::absolute(path).parent_path(), true);
    {
        SqliteDb db(temp.file.u8string());
        schema(db, "project", true);
        SqliteTxn tx(db);
        auto insert = db.prepare("INSERT INTO metadata VALUES('document',?)",
                                 "save document");
        (void)insert.bind(1, encode(saved)).step();
        for (const auto &entry : saved.patterns) {
            insertPattern(db, entry.second);
        }
        tx.commit();
    }
    replaceFile(temp.file, path, replace);
}

State loadProject(const fs::path &path) {
    if (!fs::is_regular_file(path) ||
        fs::file_size(path) > 512ULL * 1024 * 1024) {
        throw std::runtime_error("Missing or excessive project file");
    }
    SqliteDb db(path.u8string());
    schema(db, "project", false);
    auto query = db.prepare("SELECT value FROM metadata WHERE key='document'",
                            "read document");
    if (query.step() != SQLITE_ROW) {
        throw std::runtime_error("Project has no document");
    }
    auto state = decode(column(query.get(), 0));
    rebase(state, fs::absolute(path).parent_path(), false);
    return state;
}

std::vector<Pattern> loadLibrary(const fs::path &path) {
    if (path.empty() || !fs::exists(path)) {
        return {};
    }
    if (fs::file_size(path) > 512ULL * 1024 * 1024) {
        throw std::runtime_error("Pattern library exceeds size limit");
    }
    SqliteDb db(path.u8string());
    schema(db, "library", false);
    auto query = db.prepare(
        "SELECT exact,structural,payload,status FROM patterns ORDER BY exact",
        "read patterns");
    std::vector<Pattern> result;
    while (query.step() == SQLITE_ROW) {
        Pattern pattern;
        std::istringstream input(column(query.get(), 2));
        {
            cereal::JSONInputArchive archive(input);
            archive(pattern);
        }
        if (pattern.exact != column(query.get(), 0) ||
            pattern.structural != column(query.get(), 1) ||
            pattern.status != sqlite3_column_int(query.get(), 3) ||
            pattern.status < 1 || pattern.status > 2 || pattern.masks.empty() ||
            pattern.masks.size() > 32) {
            throw std::runtime_error("Invalid library pattern");
        }
        for (int mask : pattern.masks) {
            if (mask < 0 || mask > 31) {
                throw std::runtime_error("Invalid library lanes");
            }
        }
        result.push_back(std::move(pattern));
    }
    return result;
}

void promote(const fs::path &path, const Pattern &pattern) {
    if (pattern.status == 0) {
        throw std::runtime_error("Confirm a pattern before promoting it");
    }
    const bool create = !fs::exists(path);
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
    SqliteDb db(path.u8string());
    schema(db, "library", create);
    SqliteTxn tx(db);
    auto query = db.prepare("SELECT payload FROM patterns WHERE exact=?",
                            "existing decision");
    query.bind(1, pattern.exact);
    if (query.step() == SQLITE_ROW) {
        Pattern old;
        std::istringstream input(column(query.get(), 0));
        {
            cereal::JSONInputArchive archive(input);
            archive(old);
        }
        if (old.masks != pattern.masks) {
            throw std::runtime_error(
                "Library already contains a different confirmed decision; no "
                "overwrite performed");
        }
    }
    insertPattern(db, pattern);
    tx.commit();
}
} // namespace ChartStudio
