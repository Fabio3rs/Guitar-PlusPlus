#include "ObjModel.h"
#include "SceneAO.h"
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr
            << "Usage: gpp_bake_ao path/to/model.obj\nWrites model.obj.ao and "
               "model.obj.ao.pgm; does not modify OBJ/MTL.\n";
        return 1;
    }
    try {
        std::ifstream input(argv[1]);
        if (!input)
            throw std::runtime_error("cannot read OBJ");
        const auto model = ObjModel::read(input);
        const auto atlas = SceneAO::bake(model);
        SceneAO::write(argv[1], model, atlas);
        std::cout << "AO: " << atlas.width << " x " << atlas.height << ", "
                  << atlas.coordinates.size() << " corners, 32 samples/texel\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
