// Run from build/bin (runtime assets). Args: seconds players threaded bpm
// slow-ms.
#include "GPPGame.h"
#include "GameplayProfile.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

int main(int argc, char **argv) {
    const double seconds = argc > 1 ? std::max(.1, std::atof(argv[1])) : 5.0;
    const int count = argc > 2 ? std::clamp(std::atoi(argv[2]), 1, 4) : 1;
    const bool threaded = argc > 3 && std::atoi(argv[3]);
    const bool bpm = argc > 4 && std::atoi(argv[4]);
    const int slow = argc > 5 ? std::max(0, std::atoi(argv[5])) : 0;
    auto &engine = CEngine::engine();
    engine.init();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    engine.openWindow("Gameplay benchmark", 800, 600, 0);
    glfwSwapInterval(0);
    auto &game = GPPGame::GuitarPP();
    game.mainthread = std::this_thread::get_id();
    game.showTextsTest = false;
    game.usarPalheta = true;
    for (const auto &entry :
         std::initializer_list<std::pair<const char *, GPPOBJ *>>{
             {"GPP_Note.obj", &game.noteOBJ},
             {"TriggerBase.obj", &game.triggerBASEOBJ},
             {"Trigger.obj", &game.triggerOBJ},
             {"Pylmbar.obj", &game.pylmbarOBJ},
             {"GPP_Opennote.obj", &game.openNoteOBJ}}) {
        if (!game.loadModelSync("data/models", entry.first, *entry.second)) {
            std::cerr << "Missing model: " << entry.first << '\n';
            return 2;
        }
    }
    CGamePlay module;
    module.showBPMLines = bpm;
    module.bRenderHUD = argc > 6 && std::atoi(argv[6]);
    module.bFretboardLightFading = false;
    module.enableTails = true;
    for (int i = 0; i < count; ++i) {
        auto p = std::make_shared<CPlayer>("benchmark");
        p->enableBot = true;
        CChart::Note tempo;
        tempo.lTime = 180;
        p->Notes.BPM.push_back(tempo);
        for (int n = 0; n < 20000; ++n) {
            CChart::Note note;
            note.time = n * .025;
            note.type = (1 << (n % 5)) | nf_not_hopo;
            if (n % 3 == 0) {
                note.type &= ~nf_not_hopo;
            }
            if (n % 7 == 0) {
                note.type |= 3;
            }
            if (n % 11 == 0) {
                note.type |= nf_open;
            }
            if (n % 13 == 0) {
                note.type |= nf_slide;
                note.lTime = .15;
            }
            p->Notes.gNotes.push_back(note);
        }
        for (size_t n = 0; n + 15 < p->Notes.gNotes.size(); n += 160) {
            CChart::plusNote phrase;
            phrase.firstNote = n;
            phrase.lastNote = n + 15;
            phrase.time = p->Notes.gNotes[n].time;
            phrase.lTime = .4;
            p->Notes.gPlus.push_back(phrase);
        }
        p->Notes.lastNoteEnd = p->Notes.chartEnd = 500;
        module.players.push_back(std::move(p));
    }
    using Clock = std::chrono::steady_clock;
    auto render = [&]() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        module.render();
        engine.renderFrame();
        if (slow) {
            std::this_thread::sleep_for(std::chrono::milliseconds(slow));
        }
    };
    // Warm up allocations, shaders and caches on the same objects.
    module.startSongTime = Clock::now();
    module.startUpdateDelta();
    for (int i = 0; i < 100; ++i) {
        module.update();
        render();
    }
    for (auto &p : module.players) {
        // Preserve the allocated chart but reset judgments/score/Plus just as
        // at a song boundary. A partial reset leaves the bot chasing a note
        // from warmup, making the measured workload depend on warmup FPS.
        auto notes = std::move(p->Notes);
        p->resetSongThings();
        p->Notes = std::move(notes);
        p->Notes.notePos = 0;
        p->Notes.plusPos = 0;
        p->Notes.lastNotePicked = ~size_t{0};
        std::fill(std::begin(p->Notes.fretsNotePickedTime),
                  std::end(p->Notes.fretsNotePickedTime), 0.0);
        for (auto &note : p->Notes.gNotes) {
            note.type &= ~(nf_picked | nf_failed | nf_slide_picked |
                           nf_doing_slide | losted | hopontstrmmd | strmstlrc);
        }
    }
#ifdef GPP_PROFILE_GAMEPLAY
    gpp::profile::reset();
#endif
    module.startSongTime = Clock::now();
    module.startUpdateDelta();
    // Optional perf FIFO protocol: only measurement boundaries touch the pipe.
    // perf stat/record --delay=-1 --control=fifo:control,ack
    std::ofstream perfControl;
    std::ifstream perfAck;
    if (const char *path = std::getenv("GPP_PERF_CONTROL")) {
        const char *ack = std::getenv("GPP_PERF_ACK");
        if (!ack) {
            return 3;
        }
        perfControl.open(path);
        perfAck.open(ack);
    }
    auto control = [&](const char *command) {
        if (perfControl.is_open()) {
            perfControl << command << '\n' << std::flush;
            std::string ack;
            std::getline(perfAck, ack);
        }
    };
    control("enable");
    const auto begin = Clock::now();
    std::atomic<bool> done{false};
    size_t updates = 0, frames = 0;
    std::ostringstream updateReport;
    std::thread worker;
    if (threaded) {
        worker = std::thread([&]() {
#ifdef GPP_PROFILE_GAMEPLAY
            gpp::profile::reset();
#endif
            while (!done.load(std::memory_order_relaxed)) {
                module.update();
                ++updates;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
#ifdef GPP_PROFILE_GAMEPLAY
            gpp::profile::report(updateReport);
#endif
        });
    }
    while (std::chrono::duration<double>(Clock::now() - begin).count() <
           seconds) {
        if (!threaded) {
            module.update();
            ++updates;
        }
        render();
        ++frames;
    }
    done = true;
    if (worker.joinable()) {
        worker.join();
    }
    const double elapsed =
        std::chrono::duration<double>(Clock::now() - begin).count();
    control("disable");
    std::cout << "work," << elapsed << ',' << updates << ',' << frames << ','
              << count << ',' << threaded << ',' << bpm << ',' << slow << '\n';
    std::cout
        << "metric,name,n,mean_us,median_us,p95_us,p99_us,max_us,dropped\n";
    std::cout << updateReport.str();
#ifdef GPP_PROFILE_GAMEPLAY
    gpp::profile::report(std::cout);
#endif
    for (const auto &p : module.players) {
        std::cout << "result," << p->Notes.notePos << ',' << p->correctNotes
                  << ',' << p->getPoints() << '\n';
    }
    game.shutdownModels();
    game.shutdownTextures();
}
