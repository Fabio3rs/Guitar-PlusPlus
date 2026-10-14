#include "CPlayer.h"
#include "CChart.h"
#include "CEngine.h"
#include "CFonts.h"
#include "CLog.h"
#include "CMultiplayer.h"
#include "GPPGame.h"
#include "PlusRules.h"
#include "SongFiles.h"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

// static constexpr size_t PLAYSZ = sizeof(CPlayer);

/*const int CPlayer::notesEnum =
    nf_green | nf_red | nf_yellow | nf_blue | nf_orange;
const int CPlayer::notesEnumWithOpenNotes =
    nf_green | nf_red | nf_yellow | nf_blue | nf_orange | nf_open;*/

auto CPlayer::smartChartSearch(const std::string &path) -> std::string {
    return CChart::smartChartSearch(path);
}

void CPlayer::muteInstrument() const {
    CEngine::engine().setSoundVolume(instrumentSound, 0.0f);
}

void CPlayer::unmuteInstrument() const {
    CEngine::engine().setSoundVolume(instrumentSound, 1.0f);
}

std::atomic<int> palhetaNpKey;

void CPlayer::update() {
    int i = 0;
    int pklast = palhetaKeyLast;

    if ((CEngine::engine().getTime() - npPsetted) > 0.01 && enableBot) {
        palhetaNpKey = 0;
    }

    bool clearFretsP = true;

    if (enableBot) {
        std::copy(fretsPressed, fretsPressed + 5, lastFretsPressed);

        // if (GPPGame::GuitarPP().showTextsTest)
        //	CFonts::fonts().drawTextInScreen("BOT PLAYING", -1.65, -0.9,
        // 0.05);

        for (bool &i : fretsPressed) {
            i = false;
        }

        bool dngslide = false;

        if (Notes.lastNotePicked != -1 && strklinent != -1) {
            auto &note = Notes.gNotes[Notes.lastNotePicked];

            double rtime = (note.time - musicRunningTime);

            if (rtime < 0.1 && rtime > -0.1) {
                for (size_t i = 0; i < 5; ++i) {
                    if ((note.type & (int)pow(2, i)) != 0) {
                        fretsPressed[i] = true;
                        clearFretsP = false;
                    }
                }
            } else {
                for (bool &i : fretsPressed) {
                    i = false;
                }
            }
        }

        for (size_t i = 0; i < 5; ++i) {
            if (notesSlide[i] != -1) {
                fretsPressed[i] = true;
                clearFretsP = false;
                dngslide = true;
            }
        }

        if (strklinent != -1 && !dngslide) {
            auto &note = Notes.gNotes[strklinent];

            strklinenttime = (note.time - musicRunningTime);

            if (strklinenttime >= -0.05 &&
                ((strklinenttime < 0.14 &&
                  ((note.type & notesFlags::nf_not_hopo) != 0)) ||
                 (strklinenttime < 0.04))) {
                for (size_t i = 0; i < 5; ++i) {
                    if ((note.type & (int)pow(2, i)) != 0) {
                        fretsPressed[i] = true;
                    } else {
                        fretsPressed[i] = false;
                    }
                }

                if (strklinenttime < 0.04 &&
                    ((note.type & notesFlags::nf_picked) == 0) &&
                    ((note.type & notesFlags::nf_failed) == 0) &&
                    ((note.type & notesFlags::nf_doing_slide) == 0) &&
                    ((note.type & notesFlags::nf_slide_picked) == 0)) {
                    palhetaKey = 1;
                    palhetaNpKey = palhetaKey;
                    npPsetted = CEngine::engine().getTime();
                } else {
                    palhetaKey = 0;
                    palhetaKeyLast = palhetaKey;
                }
            }
        } else {
            palhetaKey = 0;
            palhetaKeyLast = palhetaKey;
        }
    } else if (!remoteControls) {
        palhetaNpKey = palhetaKey =
            (static_cast<int>(CEngine::engine().getKey(
                                  GPPGame::GuitarPP().fretOneKey) != 0) ||
             (CEngine::engine().getKey(GPPGame::GuitarPP().fretTwoKey)) != 0);

        if (pklast == palhetaKey) {
            palhetaKey = 0;
        } else {
            palhetaKeyLast = palhetaKey;
        }

        std::copy(fretsPressed, fretsPressed + 5, lastFretsPressed);

        int countI = 0;

        double timeC = CEngine::engine().getTime();

        for (auto &f : fretsPressed) {
            bool ftemp = f;
            f = (CEngine::engine().getKey(GPPGame::GuitarPP().strumKeys[i++]) !=
                 0);

            if (ftemp != f) {
                palhetaKey = 0;

                if (f) {
                    {
                        fretsPressedTime[countI] = timeC;
                    }
                }
            }

            countI++;
        }
    } else {
#ifdef COMPILEMP
        auto &pData = CMultiplayer::sgetPlData();

        CMultiplayer::playersData pdata;

        for (size_t i = 0, size = pData.size(); i < size; i++) {
            if (plname == pData[i].name) {
                // std::cout << pData[i].keys << std::endl;
                pdata = pData[i];
                break;
            }
        }

        palhetaNpKey = palhetaKey = pdata.strum;

        if (pklast == palhetaKey) {
            palhetaKey = false;
        } else {
            palhetaKeyLast = palhetaKey;
        }

        std::copy(fretsPressed, fretsPressed + 5, lastFretsPressed);

        int keys[5];

        for (int j = 0; j < 5; j++) {
            keys[j] = 0;

            if (pdata.keys & (int)pow(2, j)) {
                keys[j] = 1;
            }
        }

        double timeC = CEngine::engine().getTime();

        for (auto &f : fretsPressed) {
            bool ftemp = f;
            f = keys[i++];

            if (ftemp != f) {
                palhetaKey = false;

                if (f) {
                    fretsPressedTime[i] = timeC;
                }
            }
        }
#endif
    }
}

auto CPlayer::smartSongSearch(const std::string &path) -> std::string {
    const auto directory =
        std::filesystem::path("data/songs") / std::filesystem::u8path(path);
    try {
        return songfiles::audio(directory, songfiles::list(directory))
            .filename()
            .u8string();
    } catch (const std::filesystem::filesystem_error &) {
        return {};
    }
}

auto CPlayer::loadSongOnlyChart(const std::string &path) -> bool {
    releaseSong();
    Notes.unloadChart();
    songName.clear();
    songArtist.clear();
    songCharter.clear();
    songFullPath.clear();
    const auto chartName = smartChartSearch(path);
    if (chartName.empty()) {
        CLog::log() << "No chart found for song: " + path;
        return false;
    }
    const std::string fullFilePath = "data/songs/" + path + "/" + chartName;
    std::string extension = chartName.substr(chartName.find_last_of('.'));
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == ".mid") {
        return Notes.loadMidiChart(fullFilePath, *this);
    }
    return extension == ".chart" ? Notes.loadFeedbackChart(fullFilePath, *this)
                                 : Notes.loadChart(fullFilePath, *this);
}

auto CPlayer::loadSong(const std::string &path) -> bool {
    if (!loadSongOnlyChart(path)) {
        return false;
    }

    return loadSongAudio(path);
}

bool CPlayer::loadSongAudio(const std::string &path) {
    releaseSong();
    auto &engine = CEngine::engine();
    const std::string fullPath = "data/songs/" + path;
    const auto audioName = smartSongSearch(path);
    songFullPath = fullPath + "/" + audioName;
    // The session clock/playback uses the main song stream. Instrument stems
    // are optional and must not turn an otherwise playable song into a failure.
    if (audioName.empty() ||
        !engine.loadSoundStream(songFullPath.c_str(), songAudioID)) {
        CLog::log() << "Cannot load song audio: " + songFullPath;
        releaseSong();
        songFullPath.clear();
        return false;
    }

    const auto findStem = [&](const std::string &name) {
        for (const char *extension : {".ogg", ".mp3"}) {
            auto file = GPPGame::caseInsensitiveSearchDir(
                fullPath.c_str(), true, false, name + extension);
            if (!file.empty()) {
                return file;
            }
        }
        return std::string{};
    };
    // Difficulty does not change the audio arrangement.
    const bool bass = instrument.find("DoubleBass]") != std::string::npos;
    const std::string selected =
        bass ? (findStem("bass").empty() ? "rhythm" : "bass")
        : instrument.find("Single]") != std::string::npos ? "guitar"
                                                          : "";
    const bool splitDrums =
        !findStem("drums_1").empty() || !findStem("drums_2").empty() ||
        !findStem("drums_3").empty() || !findStem("drums_4").empty();
    const bool splitVocals =
        !findStem("vocals_1").empty() || !findStem("vocals_2").empty();
    for (const char *name :
         {"guitar", "rhythm", "bass", "keys", "drums", "drums_1", "drums_2",
          "drums_3", "drums_4", "vocals", "vocals_1", "vocals_2", "crowd"}) {
        if ((splitDrums && std::string(name) == "drums") ||
            (splitVocals && std::string(name) == "vocals")) {
            continue;
        }
        const auto file = findStem(name);
        if (file.empty() || file == audioName) {
            continue;
        }
        int handle = 0;
        if (!engine.loadSoundStream((fullPath + "/" + file).c_str(), handle)) {
            engine.unloadSoundStream(handle);
            CLog::log() << "Cannot load optional instrument audio: " + file;
            continue;
        }
        if (selected == name) {
            instrumentSound = handle;
            instrumentAudioName = name;
        } else {
            backingStems.emplace(name, handle);
        }
    }
    return true;
}

auto CPlayer::getLevel() const -> int {
    return static_cast<int>(log(experience));
}

void CPlayer::resetData() {
    correctNotes = 0;
    bRenderP = bUpdateP = true;
    bPlusStrike = false;

    updatedMusicRunningTime = 0.0;
    musicRunningTime = 0.0;
    std::fill(std::begin(notesSlide), std::end(notesSlide), -1);
    std::fill(std::begin(lastFretsPressed), std::end(lastFretsPressed), false);
    std::fill(std::begin(fretsPressed), std::end(fretsPressed), false);
    std::fill(std::begin(fretsPressedTime), std::end(fretsPressedTime), 0.0);
}

void CPlayer::resetCamera() {
    playerCamera.eye.x = 0.0;
    playerCamera.eye.y = 0.5;
    playerCamera.eye.z = 2.42;
    playerCamera.center.x = 0.0;
    playerCamera.center.y = -0.2;
    playerCamera.center.z = 0.0;
    playerCamera.up.x = 0.0;
    playerCamera.up.y = 1.0;
    playerCamera.up.z = 0.0;

    targetCamera = playerCamera;
}

void CPlayer::addPointsByDoingLongNote(double activeSeconds) {
    if (!std::isfinite(activeSeconds) || activeSeconds <= 0.0) {
        return;
    }
    const double earned = comboToMultiplier() * 50.0 * activeSeconds;
    points += earned;
    // Provisional XP rate: 0.5 * multiplier per active second. XP progression,
    // balance and persistence have no settled architecture yet; only remove
    // tick-rate dependence here, without establishing campaign rewards.
    experience += earned / 100.0;
}

void CPlayer::processError() {
    breakCombo();
    breakPlus();
    aError = true;
    muteInstrument();

    if (publicAprov > 0) {
        {
            publicAprov--;
        }
    }
}

void CPlayer::processErrorNonPickedB(size_t pos) {
    double currentPlusStart = -1.0;
    double currentPlusEnd = -1.0;
    if (Notes.plusPos < Notes.gPlus.size()) {
        auto &plusp = Notes.gPlus[Notes.plusPos];
        currentPlusStart = plusp.time;
        currentPlusEnd = plusp.time + plusp.lTime;
    }

    bool shouldBreakPlus = false;

    if (pos > 0 && pos < Notes.gNotes.size()) {
        auto &noteRef = Notes.gNotes[pos];
        double tMin = noteRef.time - 0.5;

        pos--;

        int count = 0;

        for (int64_t i = pos; i >= 0; i--) {
            auto &note = Notes.gNotes[i];
            if (note.time > tMin &&
                ((note.type & notesFlags::nf_picked) == 0) &&
                ((note.type & notesFlags::nf_failed) == 0) &&
                ((note.type & notesFlags::nf_doing_slide) == 0)) {
                note.type |= notesFlags::nf_failed;
                ++count;

                if (note.time >= currentPlusStart &&
                    note.time < currentPlusEnd) {
                    shouldBreakPlus = true;
                }
            }

            if (note.time < tMin) {
                break;
            }
        }

        if (count > 0) {
            breakCombo();

            if (count > publicAprov) {
                publicAprov = 0;
            } else {
                publicAprov -= count;
            }
        }
    }

    if (shouldBreakPlus) {
        ++Notes.plusPos;
        plusLoadF = plusLoadB = 0.0;
    }
}

void CPlayer::releaseSong() {
    if (songAudioID != -1 && (songAudioID != 0)) {
        CEngine::engine().unloadSoundStream(songAudioID);
    }

    if (instrumentSound != -1 && (instrumentSound != 0)) {
        CEngine::engine().unloadSoundStream(instrumentSound);
    }

    for (auto &stem : backingStems) {
        CEngine::engine().unloadSoundStream(stem.second);
    }
    backingStems.clear();
    activeAudioStems.clear();
    instrumentAudioName.clear();
    songAudioID = -1;
    instrumentSound = 0;

    BPMNowBuffer = 0;
}

void CPlayer::breakCombo() { combo = 0; }

void CPlayer::breakPlus() {
    if (Notes.plusPos >= Notes.gPlus.size()) {
        return;
    }

    auto &plusp = Notes.gPlus[Notes.plusPos];

    if (musicRunningTime < plusp.time ||
        musicRunningTime > (plusp.time + plusp.lTime)) {
        return;
    }

    ++Notes.plusPos;
    plusLoadF = plusLoadB = 0.0;
}

auto CPlayer::comboToMultiplierWM() -> double { return comboToMultiplier(); }

void CPlayer::takeLoadedSong(CPlayer &source) {
    if (&source == this) {
        return;
    }
    releaseSong();
    Notes = std::move(source.Notes);
    songAudioID = std::exchange(source.songAudioID, -1);
    instrumentSound = std::exchange(source.instrumentSound, -1);
    backingStems = std::move(source.backingStems);
    source.backingStems.clear();
    activeAudioStems = std::move(source.activeAudioStems);
    instrumentAudioName = std::move(source.instrumentAudioName);
    backingVolume = source.backingVolume;
    songName = std::move(source.songName);
    songArtist = std::move(source.songArtist);
    songCharter = std::move(source.songCharter);
    songFullPath = std::move(source.songFullPath);
    correctNotesMarathon += correctNotes;
    correctNotes = 0;
    strklinent = -1;
    BPMNowBuffer = 0;
    for (auto &slide : notesSlide) {
        slide = ~0uL;
    }
}

void CPlayer::beginSongResult() {
    songScoreStart = points;
    songCorrectStart = correctNotes;
    songMaxCombo = 0;
}
PlayerSongResult CPlayer::songResult() const {
    return {static_cast<int64_t>(points - songScoreStart), songMaxCombo,
            correctNotes - songCorrectStart,
            static_cast<uint64_t>(Notes.gNotes.size())};
}

auto CPlayer::getCombo() const -> int64_t { return combo; }

auto CPlayer::getPoints() const -> int64_t {
    return static_cast<int64_t>(points);
}

auto CPlayer::isSongChartFinished() const -> bool {
    return Notes.gNotes.empty() || musicRunningTime > Notes.lastNoteEnd + 3.0;
}

auto CPlayer::comboToMultiplier() const -> double {
    double result = double(combo) / 10.0;

    result = result < 1.0 ? 1.0 : result;
    result = result > 4.0 ? 4.0 : result;

    return result * (static_cast<double>(plusEnabled) + 1.0);
}

auto CPlayer::getLastFretsPressedFlags() const -> int {
    int result = 0;
    for (size_t i = 0; i < 5; ++i) {
        if (lastFretsPressed[i]) {
            result |= static_cast<int>(pow(2, i));
        }
    }

    return result;
}

auto CPlayer::getFretsPressedFlags() const -> int {
    int result = 0;
    for (size_t i = 0; i < 5; ++i) {
        if (fretsPressed[i]) {
            result |= static_cast<int>(pow(2, i));
        }
    }

    return result;
}

void CPlayer::doNote(size_t i) {
    if ((Notes.gNotes[i].type & notesFlags::nf_picked) == 0) {
        ++combo;
        songMaxCombo = std::max(songMaxCombo, combo);
        ++correctNotes;

        Notes.lastNotePicked = i;

        points += comboToMultiplier() * 50.0;

        publicAprov++;

        aError = false;

        unmuteInstrument();

        experience += 50.0 * comboToMultiplier() / 100.0;

        if (publicAprov > maxPublicAprov) {
            publicAprov = maxPublicAprov;
        }
    }
}

void CPlayer::playSongAudio(const std::vector<std::string> &activeStems) {
    activeAudioStems = activeStems;
    setSongVolume(backingVolume);
    CEngine::engine().playSoundStream(songAudioID);
    for (const auto &stem : backingStems) {
        CEngine::engine().playSoundStream(stem.second);
    }
}

void CPlayer::pauseSongAudio() const {
    CEngine::engine().pauseSoundStream(songAudioID);
    for (const auto &stem : backingStems) {
        CEngine::engine().pauseSoundStream(stem.second);
    }
}

void CPlayer::seekSongAudio(double seconds) const {
    CEngine::engine().setSoundTime(songAudioID, seconds);
    for (const auto &stem : backingStems) {
        CEngine::engine().setSoundTime(stem.second, seconds);
    }
}

void CPlayer::setSongVolume(float volume) {
    backingVolume = volume;
    CEngine::engine().setSoundVolume(songAudioID, volume);
    for (const auto &stem : backingStems) {
        const bool played =
            std::find(activeAudioStems.begin(), activeAudioStems.end(),
                      stem.first) != activeAudioStems.end();
        CEngine::engine().setSoundVolume(stem.second, played ? 0.0f : volume);
    }
}

void CPlayer::instrumentPlay() const {
    CEngine::engine().playSoundStream(instrumentSound);
}

void CPlayer::instrumentPause() const {
    CEngine::engine().pauseSoundStream(instrumentSound);
}

CPlayer::CPlayer(const char *name) {
    bPlusStrike = false;
    usarPalheta = GPPGame::GuitarPP().usarPalheta;
    correctNotesMarathon = 0;
    guitar = nullptr;
    npPsetted = 0;
    playerHudOffsetX = playerHudOffsetY = 0.0;
    remoteControls = false;
    bRenderP = bUpdateP = true;
    plname = name;
    songAudioID = -1;
    points = 0.0;
    combo = 0;
    startTime = CEngine::engine().getTime();
    plusEnabled = false;
    musicRunningTime = 0.0;
    rangle = 0;
    palhetaKeyLast = palhetaKey = 0;
    aError = false;
    lastHOPO = 0;
    enableBot = false;

    updatedMusicRunningTime = 0.0;

    instrumentSound = 0;
    strklinenttime = -5.0;

    BPMNowBuffer = 0;

    plusThunterStrikeStart = plusPower = plusParticleEffectPosition =
        maxPlusPower = plusLoadF = plusLoadB = plusLoadInterval = 0.0;
    plusCircleBuffer = plusLoadBuffer = publicApprovBuffer =
        correctNotesBuffer = multiplierBuffer = ~0U;

    correctNotes = 0;

    maxPlusPower = gpp::plus::maximumEnergy;
    maxPublicAprov = 120.0;
    publicAprov = maxPublicAprov / 2.0;

    strklinent = ~(static_cast<size_t>(0));

    resetCamera();
    resetData();

    experience = 100.0;
    tailsData.reserve(32);

    instrument = "[ExpertSingle]";
}

CPlayer::~CPlayer() noexcept { releaseSong(); }

void CPlayer::resetSongThings() {
    releaseSong();
    // buffer.clear();
    Notes.unloadChart();

    songAudioID = -1;
    points = 0.0;
    combo = 0;

    correctNotesMarathon = 0;

    resetData();
    instrumentSound = 0;
    strklinenttime = -5.0;

    BPMNowBuffer = 0;

    plusThunterStrikeStart = plusPower = plusParticleEffectPosition =
        maxPlusPower = plusLoadF = plusLoadB = plusLoadInterval = 0.0;
    plusCircleBuffer = plusLoadBuffer = publicApprovBuffer =
        correctNotesBuffer = multiplierBuffer = ~0uL;

    correctNotes = 0;

    maxPlusPower = gpp::plus::maximumEnergy;
    maxPublicAprov = 120.0;
    publicAprov = maxPublicAprov / 2.0;

    correctNotes = 0;
    strklinent = ~(static_cast<size_t>(0));
    musicRunningTime = 0.0;
    points = 0.0;
    combo = 0;
    plusEnabled = false;
    bPlusStrike = false;
}
