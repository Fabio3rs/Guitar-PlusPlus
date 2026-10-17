#include "CCharter.h"
#include "CFonts.h"
#include "GPPGame.h"
#include <algorithm>
#include <bass.h>
#include <bass_fx.h>
#include <cmath>
#include <memory>
#include <stdexcept>

CCharter::CCharter(GPPGame &game) : songPath(game.songToLoad) {
    gpModule.players.push_back(std::make_unique<CPlayer>("guitar charter"));
    auto &player = *gpModule.players.back();
    if (!player.loadSong(game.songToLoad)) {
        player.loadSongAudio(game.songToLoad);
    }
    if (player.Notes.BPM.empty()) {
        CChart::Note tempo;
        tempo.lTime = 120;
        player.Notes.BPM.push_back(tempo);
    }
    gpp::charter::Document document;
    constexpr unsigned int persistentFlags =
        31 | nf_not_hopo | nf_slide | noteTap | nf_open | nf_reverseStrumHopo;
    for (const auto &note : player.Notes.gNotes) {
        document.insert(
            {0, note.time, note.lTime, note.type & persistentFlags});
    }
    editor.reset(std::move(document));
    gpModule.bIsACharterGP = true;
    gpModule.bRenderHUD = false;
    gpModule.showBPMVlaues = true;
    gpModule.showBPMLines = true;
}

CCharter::~CCharter() {
    continueLoading = false;
    if (loadThread.joinable()) {
        loadThread.join();
    }
}

void CCharter::synchronizeNotes() {
    auto &notes = gpModule.players.back()->Notes;
    std::vector<CChart::Note> snapshot;
    snapshot.reserve(editor.document().notes.size());
    for (const auto &edited : editor.document().notes) {
        if ((edited.flags & (31 | nf_open)) == 0) {
            continue;
        }
        CChart::Note note;
        note.time = edited.time;
        note.lTime = edited.duration;
        note.type = edited.flags;
        if (note.lTime > 0) {
            note.type |= nf_slide;
        } else {
            note.type &= ~nf_slide;
        }
        const int lanes = note.type & 31;
        if ((lanes & (lanes - 1)) != 0) {
            note.type |= nf_not_hopo;
        }
        snapshot.push_back(note);
    }
    notes.gNotes = std::move(snapshot);
    notes.notePos = 0;
    notes.plusPos = 0;
    notes.chartEnd = 0;
    notes.lastNoteEnd = 0;
    notes.source =
        {}; // Imported ticks no longer describe a seconds-domain edit.
    for (const auto &note : notes.gNotes) {
        notes.lastNoteEnd = std::max(notes.lastNoteEnd, note.time + note.lTime);
    }
    for (auto &phrase : notes.gPlus) {
        phrase.firstNote = ~0uLL;
        phrase.lastNote = 0;
    }
    notes.deducePlusLastNotes();
}

void CCharter::prepareDemoGamePlay(CGamePlay &gp) {
    workingNote.reset();
    synchronizeNotes();
    for (const auto &player : gpModule.players) {
        auto copy = std::make_unique<CPlayer>("charter preview");
        copy->instrument = player->instrument;
        copy->loadSongAudio(songPath);
        copy->Notes = player->Notes;
        copy->songName = player->songName;
        copy->songArtist = player->songArtist;
        copy->musicRunningTime = atMusicTime;
        copy->enableBot = true;
        gp.players.push_back(std::move(copy));
    }
}

double CCharter::beatSeconds() const {
    double bpm = 120;
    for (const auto &tempo : gpModule.players.back()->Notes.BPM) {
        if (tempo.time > atMusicTime) {
            break;
        }
        if (tempo.lTime > 0 && std::isfinite(tempo.lTime)) {
            bpm = tempo.lTime;
        }
    }
    return 60.0 / bpm;
}

void CCharter::preRender() {
    auto &engine = CEngine::engine();
    auto &player = *gpModule.players.back();
    player.musicRunningTime = atMusicTime;
    gpModule.update();
    const bool forward = engine.getKey(GLFW_KEY_UP) != 0;
    const bool backward = engine.getKey(GLFW_KEY_DOWN) != 0;
    const bool left = engine.getKey(GLFW_KEY_LEFT) != 0;
    const bool right = engine.getKey(GLFW_KEY_RIGHT) != 0;
    if ((left || right) && !divisionHeld) {
        divTime = std::clamp(divTime * (right ? 2 : .5), .125, 4.0);
    }
    divisionHeld = left || right;
    if ((forward != backward) && !forwardHeld) {
        std::vector<gpp::charter::TempoPoint> tempos;
        tempos.reserve(player.Notes.BPM.size());
        for (const auto &tempo : player.Notes.BPM) {
            tempos.push_back({tempo.time, tempo.lTime});
        }
        atMusicTime = std::max(
            0.0, gpp::charter::advanceBeats(tempos, atMusicTime,
                                            (forward ? 1 : -1) * divTime));
        player.Notes.notePos = 0;
        player.Notes.plusPos = 0;
        player.renderState.BPMNowBuffer = 0;
    }
    forwardHeld = forward || backward;
    const bool analysis = engine.getKey(GLFW_KEY_B) != 0;
    if (analysis && !analysisHeld && !loading) {
        startAnalysis();
    }
    analysisHeld = analysis;
    const bool apply = engine.getKey(GLFW_KEY_A) != 0;
    if (apply && !applyHeld && bpmReady && !loading) {
        std::lock_guard<std::mutex> lock(loadBPMMutex);
        if (!songBPM.empty()) {
            player.Notes.BPM = songBPM;
            player.renderState.BPMNowBuffer = 0;
        }
        bpmReady = false;
    }
    applyHeld = apply;
    const bool ctrl = engine.getKey(GLFW_KEY_LEFT_CONTROL) ||
                      engine.getKey(GLFW_KEY_RIGHT_CONTROL);
    const bool undo = ctrl && engine.getKey(GLFW_KEY_Z);
    const bool redo = ctrl && engine.getKey(GLFW_KEY_Y);
    if ((undo && !undoHeld && editor.undo()) ||
        (redo && !redoHeld && editor.redo())) {
        workingNote.reset();
        synchronizeNotes();
    }
    undoHeld = undo;
    redoHeld = redo;
    const unsigned int frets = player.getFretsPressedFlags() & 31;
    if (!frets) {
        workingNote.reset();
    }
    if (frets && frets != previousFrets && !ctrl) {
        const auto existing =
            editor.document().nearest(atMusicTime, beatSeconds() / 32.0);
        if (existing) {
            workingNote = existing;
            editor.edit(
                [&](auto &document) { document.lanes(*existing, frets); });
        } else {
            editor.edit([&](auto &document) {
                workingNote = document.insert({0, atMusicTime, 0, frets});
            });
        }
        synchronizeNotes();
    } else if (frets && workingNote) {
        const auto *note = editor.document().find(*workingNote);
        if (!note || atMusicTime < note->time) {
            workingNote.reset();
        } else if (atMusicTime - note->time > .05 &&
                   std::abs(note->duration - (atMusicTime - note->time)) >
                       1e-9) {
            const double duration = atMusicTime - note->time;
            editor.edit([&](auto &document) {
                document.sustain(*workingNote, duration);
            });
            synchronizeNotes();
        }
    }
    previousFrets = frets;
    player.musicRunningTime = atMusicTime;
    player.Notes.BPMMinPosition =
        player.Notes.BPM.empty() ? 0 : player.Notes.BPM.front().time;
}

void CCharter::startAnalysis() {
    if (loadThread.joinable()) {
        loadThread.join();
    }
    const auto path = gpModule.players.back()->songFullPath;
    if (path.empty()) {
        return;
    }
    continueLoading = true;
    loading = true;
    bpmReady = false;
    readBPMPercent = 0;
    {
        std::lock_guard<std::mutex> lock(loadBPMMutex);
        songBPM.clear();
        analysisError.clear();
    }
    try {
        loadThread = std::thread(&CCharter::readSongBPM, this, path);
    } catch (...) {
        loading = false;
        continueLoading = false;
        throw;
    }
}

void CCharter::readSongBPM(const std::string &path) {
    // This worker owns a decode-only stream and never calls OpenGL or mutates
    // player data.
    const HSTREAM stream =
        BASS_StreamCreateFile(FALSE, path.c_str(), 0, 0, BASS_STREAM_DECODE);
    std::vector<CChart::Note> result;
    std::string error;
    try {
        if (!stream) {
            error = "Cannot open BPM analysis audio";
        } else {
            const double length = BASS_ChannelBytes2Seconds(
                stream, BASS_ChannelGetLength(stream, BASS_POS_BYTE));
            double previous = 0;
            for (double at = 0; continueLoading && at < length; at += 10) {
                // Synchronous decoding on this worker on both platforms. No
                // BASS_FX_FREESOURCE.
                const double bpm = BASS_FX_BPM_DecodeGet(
                    stream, at, std::min(at + 10, length), 0, BASS_FX_BPM_MULT2,
                    nullptr, nullptr);
                if (std::isfinite(bpm) && bpm > 0 &&
                    std::abs(previous - bpm) > .01) {
                    CChart::Note tempo;
                    tempo.time = at;
                    tempo.lTime = bpm;
                    result.push_back(tempo);
                    previous = bpm;
                }
                readBPMPercent =
                    length > 0 ? 100 * std::min(at + 10, length) / length : 100;
            }
        }
    } catch (const std::exception &exception) {
        result.clear();
        error = exception.what();
    } catch (...) {
        result.clear();
        error = "BPM analysis failed";
    }
    if (stream) {
        BASS_FX_BPM_Free(stream);
        BASS_StreamFree(stream);
    }
    {
        std::lock_guard<std::mutex> lock(loadBPMMutex);
        if (continueLoading) {
            songBPM = std::move(result);
            analysisError = std::move(error);
            bpmReady = !songBPM.empty();
        }
    }
    loading = false;
}

void CCharter::render() { gpModule.render(); }

void CCharter::renderInfo() {
    CFonts::fonts().drawTextInScreen("div x " + std::to_string(divTime), .5, .3,
                                     .1);
    std::string message =
        "B: analyze BPM | A: apply result | Ctrl+Z/Y: undo/redo";
    if (loading) {
        message =
            "Analyzing BPM: " + std::to_string(readBPMPercent.load()) + "%";
    } else if (bpmReady) {
        message = "BPM ready. A: replace tempo map";
    } else {
        std::lock_guard<std::mutex> lock(loadBPMMutex);
        if (!analysisError.empty()) {
            message = analysisError;
        }
    }
    CFonts::fonts().drawTextInScreen(message, -1.4, .3, .035);
}

void CCharter::renderAll() {
    preRender();
    render();
    renderInfo();
}
