#include "CCharter.h"
#include "CFonts.h"
#include <cstdint>
#include <memory>

auto CCharter::songTimePassBPMBased(const BPM_t &BPMContainer, double pass,
                                    bool forward) -> double {

    return 0.0;
}

void CCharter::prepareDemoGamePlay(CGamePlay &gp) {
    workingNote = nullptr;
    processNewNote = true;

    for (auto &p : gpModule.players) {
        eraseNulls(*p);
        (*p).Notes.deducePlusLastNotes();
    }

    for (const auto &np : gpModule.players) {
        CPlayer n = *np;

        n.enableBot = true;
        gp.players.push_back(std::make_unique<CPlayer>(n));
    }
}

void CCharter::eraseNulls(CPlayer &player) {
    for (auto it = player.Notes.gNotes.begin(); it != player.Notes.gNotes.end();
         /******/) {
        int ncount = 0;
        for (int ji : CGamePlay::notesFlagsConst) {
            if (((*it).type & ji) != 0) {
                ++ncount;
            }
        }

        if (ncount > 1) {
            (*it).type |= nf_not_hopo;
        }

        if (((*it).type & CPlayer::notesEnum) != 0) {
            ++it;
        } else {
            it = player.Notes.gNotes.erase(it);
        }
    }
}

void CCharter::preRender() {
    auto &player = *gpModule.players.back();
    {
        if (player.BPMNowBuffer > 0) {
            if (static_cast<size_t>(player.BPMNowBuffer) <
                player.Notes.BPM.size()) {
                if (player.Notes.BPM[player.BPMNowBuffer].time >
                    (atMusicTime - 2.0)) {
                    --player.BPMNowBuffer;

                    if (player.BPMNowBuffer >= 0) {
                        player.Notes.BPMMinPosition =
                            player.Notes.BPM[player.BPMNowBuffer].time;
                    }
                }
            } else {
                player.BPMNowBuffer = 0;
                player.Notes.BPMMinPosition =
                    player.Notes.BPM[player.BPMNowBuffer].time;
            }
        }

        if (player.BPMNowBuffer < 0) {
            player.BPMNowBuffer = 0;
            player.Notes.BPMMinPosition =
                player.Notes.BPM[player.BPMNowBuffer].time;
        }
    }

    if (loading) {
        {
            std::lock_guard<std::mutex> lk(loadBPMMutex);

            if (!songBPM.empty()) {
                {
                    player.Notes.BPM = songBPM;
                }
            }

            player.BPMNowBuffer = 0;
        }

        double time = readBPMAtSeconds - 2.0;

        if (time < 0.0) {
            {
                time = 0.0;
            }
        }

        atMusicTime = time;
        player.musicRunningTime = atMusicTime;
        player.Notes.plusPos = 0;
        backToZero = true;
    } else if (backToZero) {
        if (atMusicTime > 0.0) {
            atMusicTime -= CEngine::engine().getDeltaTime() * 40.0;

            if (atMusicTime < 0.0) {
                atMusicTime = 0.0;
                backToZero = false;
                player.Notes.plusPos = 0;
            }
        } else {
            backToZero = false;
            player.Notes.plusPos = 0;
        }
    }

    if (!loading) {
        bool kfor = CEngine::engine().getKey(GLFW_KEY_UP) != 0;
        bool kback = CEngine::engine().getKey(GLFW_KEY_DOWN) != 0;
        bool kleft = CEngine::engine().getKey(GLFW_KEY_LEFT) != 0;
        bool kright = CEngine::engine().getKey(GLFW_KEY_RIGHT) != 0;

        auto roundTo = [](double t, double d) {
            double d0 = pow(10.0, d);
            t *= d0;
            t = floor(t);
            return t / d0;
        };

        auto &processingP = *gpModule.players.back();

        double BPM = 120.0 / 60.0;

        if (processingP.Notes.BPM.size() > 1) {
            BPM = 30.0 / gpModule.getBPMAt(processingP, atMusicTime);
        }

        BPM = roundTo(BPM, 2);

        double BPMDiv32 = BPM / 32.0;
        // std::cout << BPMDiv64 << "   " << BPM * 0.125 << std::endl;

        if (kleft || kright) {
            if (!bLeftRightK) {
                if (kright) {
                    if (divTime < 4.0) {
                        divTime *= 2.0;
                    }
                } else {
                    if (divTime > 0.125) {
                        divTime /= 2.0;
                    }
                }
                bLeftRightK = true;
            }
        } else {
            bLeftRightK = false;
        }

        if (kfor != kback) {
            if (!bForwardBackwardK) {
                if (kfor) {
                    {
                        atMusicTime += divTime * BPM;
                    }
                } else {
                    if (atMusicTime > 0.0) {
                        {
                            atMusicTime -= divTime * BPM;
                        }
                    }

                    if (atMusicTime < 0.0) {
                        {
                            atMusicTime = 0.0;
                        }
                    }
                }
                bForwardBackwardK = true;
            }
        } else {
            bForwardBackwardK = false;
        }

        for (auto &pp : gpModule.players) {
            auto &p = *pp;
            p.musicRunningTime = atMusicTime;

            auto &notes = p.Notes;
            auto &gNotes = p.Notes.gNotes;
            auto &engine = CEngine::engine();

            while (true) {
                if (notes.plusPos > 0 && notes.plusPos < notes.gPlus.size()) {
                    auto pp = notes.plusPos - 1;

                    auto &plPos = notes.gPlus[notes.plusPos];

                    if ((plPos.time + plPos.lTime) > (atMusicTime - 2.0)) {
                        notes.plusPos = pp;
                    }
                }

                if (notes.notePos > 0) {
                    auto np = notes.notePos - 1;

                    auto &gnote = gNotes[np];

                    if ((gnote.time + gnote.lTime) > (atMusicTime - 2.0)) {
                        notes.notePos = np;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
        }

        typedef std::vector<CPlayer::NotesData::Note> notesType;

        auto searchNoteAt = [roundTo,
                             BPMDiv32](notesType &gNotes, double time,
                                       notesType::iterator &beforeTime,
                                       notesType::iterator &afterTime) {
            auto begin = gNotes.begin();
            auto it = gNotes.begin();
            auto lit = it;
            auto end = gNotes.begin();

            time = roundTo(time, 3);

            for (; it != gNotes.end(); it++) {
                auto &note = *it;

                if (note.time < time) {
                    beforeTime = it;
                }

                if (note.time > time) {
                    afterTime = it;
                }

                double deltaNotes = note.time - time;

                if (deltaNotes > (-BPMDiv32) && deltaNotes < BPMDiv32) {
                    lit = it;
                    lit++;
                    afterTime = lit;
                    break;
                }

                lit = it;
            }

            return it;
        };

        auto &gNotes = processingP.Notes.gNotes;

        notesType::iterator beforeTime = gNotes.begin();
        notesType::iterator afterTime = gNotes.end();
        notesType::iterator result;

        auto check = [&](notesType::iterator it, notesType::iterator end) {
            if (it != end) {
                std::cout << "  " << (*it).time << "  ";
            } else {
                std::cout << " it fail ";
            }
        };

        int nowkeys = processingP.getFretsPressedFlags();

        if (nowkeys == 0) {
            workingNote = nullptr;
            processNewNote = true;
        }

        if ((workingNote != nullptr) &&
            (workingNoteIt->type & CPlayer::notesEnum) !=
                processingP.getLastFretsPressedFlags()) {
            workingNote = nullptr;
        }

        bool addedNow = false;

        if (nowkeys != processingP.getLastFretsPressedFlags()) {
            result = searchNoteAt(gNotes, atMusicTime, beforeTime, afterTime);
        }

        if (nowkeys > 0 && nowkeys != processingP.getLastFretsPressedFlags() &&
            workingNote == nullptr && processNewNote) {
            if (result == gNotes.end()) {
                CPlayer::NotesData::Note newNote;
                newNote.time = atMusicTime;
                newNote.lTime = 0.0;
                newNote.type = processingP.getFretsPressedFlags();

                if (atMusicTime > processingP.Notes.chartEnd)
                    processingP.Notes.chartEnd = 0;

                workingNoteIt = gNotes.insert(beforeTime, newNote);
                result = workingNoteIt;
                workingNote = &(*workingNoteIt);
                addedNow = true;
            } else {
                workingNote = &(*result);
                workingNoteIt = result;
            }
        } else {
            if (workingNote != nullptr) {
                double deltaT = atMusicTime - (workingNoteIt->time);
                if (deltaT > 0.05) {
                    workingNoteIt->lTime = atMusicTime - (workingNoteIt->time);
                    workingNoteIt->type |= nf_slide;
                    processNewNote = false;
                } else if (deltaT < 0.0) {
                    workingNote = nullptr;
                } else if ((workingNoteIt->type & CPlayer::notesEnum) ==
                           processingP.getLastFretsPressedFlags()) {
                    workingNoteIt->type &= ~(nf_slide);
                    workingNoteIt->lTime = 0;
                }
            }
        }

        for (int i = 0; i < 5; i++) {
            if (!addedNow) {
                if (!fretKeys[i] && processingP.fretsPressed[i]) {
                    result->type ^= CGamePlay::notesFlagsConst[i];
                }
            }
            fretKeys[i] = processingP.fretsPressed[i];
        }

        if (addedNow) {
            std::sort(gNotes.begin(), gNotes.end());
        }

        /*if (nowkeys != processingP.getLastFretsPressedFlags() && atMusicTime >
        0.0 && keypts < nowkeys)
        {
        keypts = nowkeys;
        result = searchNoteAt(gNotes, atMusicTime, beforeTime, afterTime);

        CPlayer::NotesData::Note newNote;
        newNote.time = atMusicTime;
        newNote.lTime = 0.0;
        newNote.type = processingP.getFretsPressedFlags();
        newNote.type |= nf_not_hopo;

        if (result == gNotes.end())
        {
        gNotes.insert(beforeTime, newNote);
        }
        else
        {
        if (processingP.getFretsPressedFlags() != 0) result->type ^=
        processingP.getFretsPressedFlags();
        }

        std::sort(gNotes.begin(), gNotes.end());*/
        /*check(result, gNotes.end());
        check(beforeTime, gNotes.end());
        check(afterTime, gNotes.end());*/
        /*std::cout << std::endl;
}
else
{
keypts = 0;
}
*/
    }

    gpModule.update();
}

void CCharter::readSongBPM(unsigned int song) {
    songBPM.clear();
    double len = CEngine::engine().getChannelLength(song);
    double lenl = len - 0.5;

    double lastBPM = 120.0;

    unsigned int addedBPM = 0;

    for (double l = 0.0; l < lenl; l += 0.5) {
        readBPMPercent = 100.0 / len * l;
        readBPMAtSeconds = l;

        double BPM = CEngine::engine().getSoundBPM(
            songAudioID, l, (l + 10.0) <= lenl ? 10.0 : (len - l));

        if (BPM < 80.0) {
            {
                BPM *= 2.0;
            }
        }

        double ceilBPM = ceil(BPM);

        if (ceilBPM <= 0.0) {
            ceilBPM = lastBPM;
        }

        if (lastBPM != ceilBPM || addedBPM == 0) {
            lastBPM = ceilBPM;

            CPlayer::NotesData::Note newBPM;

            newBPM.time = l;
            newBPM.lTime = ceilBPM;

            std::lock_guard<std::mutex> lk(loadBPMMutex);
            songBPM.push_back(newBPM);
            ++addedBPM;
        }
    }
}

void CCharter::render() { gpModule.render(); }

auto CCharter::getBPMAt(double time) -> double {
    double result = 120.0;

    for (auto &BPMn : songBPM) {
        if (BPMn.time <= time) {
            result = BPMn.lTime;
        } else {
            break;
        }
    }

    return result;
}

void CCharter::renderInfo() {
    // CEngine::engine().setSoundTime(songAudioID, atMusicTime);
    CFonts::fonts().drawTextInScreen("div x " + std::to_string(divTime), 0.5,
                                     0.3, 0.1);
    // CFonts::fonts().drawTextInScreen("BPM " +
    // std::to_string(gpModule.getBPMAt(gpModule.players.back(), atMusicTime)),
    // -1.4, 0.3, 0.1); CFonts::fonts().drawTextInScreen("Song BPM " +
    // std::to_string(getBPMAt(atMusicTime)), -1.4, 0.15, 0.1);

    if (!loading) {
        auto &player = *gpModule.players.back();
        if (songBPM.size() != player.Notes.BPM.size()) {
            player.Notes.BPM = songBPM;
        }
    } else {
        CFonts::fonts().drawTextInScreen(
            "Loading... " + std::to_string(readBPMPercent) + "%", -1.4, 0.3,
            0.05);
    }
}

void CCharter::renderAll() {
    // auto &chartInstrument = chart.getInstrument(instrument);
    // auto &BPM = chart.getBPMContainer();

    auto &player = *gpModule.players.back();

    if (player.Notes.BPM.empty()) {
        CPlayer::NotesData::Note n;
        n.time = 0.0;
        n.lTime = 120.0;
        n.type = 0;

        player.Notes.BPM.push_back(n);
    }

    if (songBPM.empty()) {
        songBPM = player.Notes.BPM;
    }

    preRender();
    render();
    renderInfo();
}

void CCharter::loadThreadFun(CCharter &ch) {
    ch.readSongBPM(ch.songAudioID);

    ch.loading = false;
    ch.loadThreadEnd = true;
}

void CCharter::internalLoad() {
    loading = true;
    loadThreadEnd = false;
    continueLoading = true;

    loadThread = std::thread(loadThreadFun, std::ref(*this));
}

CCharter::~CCharter() {
    continueLoading = false;

    while (!loadThreadEnd) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (loadThread.joinable()) {
        {
            loadThread.join();
        }
    }
}

CCharter::CCharter() {
    readBPMPercent = 0.0;
    workingNote = nullptr;
    bForwardBackwardK = false;
    bLeftRightK = false;
    processNewNote = true;
    continueLoading = true;
    loadThreadEnd = true;
    loading = false;
    atMusicTime = 0.0;
    gpModule.players.push_back(
        std::make_unique<CPlayer>(CPlayer("guitar charter")));
    instrument = "[ExpertSingle]";
    gpModule.bIsACharterGP = true;
    divTime = 0.5;
    readBPMAtSeconds = 0.0;
    backToZero = false;

    std::string song = GPPGame::GuitarPP().songToLoad;
    // module.players[0].loadSongOnlyChart(song);
    // module.players[1].loadSong(song);
    auto &player = *gpModule.players.back();
    player.loadSong(song);
    std::cout << "audio id "
              << CEngine::engine().loadSoundStream(
                     player.songFullPath.c_str(), songAudioID, true)
              << std::endl;
    gpModule.bRenderHUD = false;
    gpModule.showBPMVlaues = true;
    gpModule.showBPMLines = true;

    for (auto &b : fretKeys) {
        b = false;
    }
    keypts = 0;

    internalLoad();
}
