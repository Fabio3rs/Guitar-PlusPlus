#include "GPPGame.h"

void exec() {
    for (auto &[name, value] : GPPGame::GuitarPP().gameModules) {
        std::cout << name  << std::endl;
    }
}
