#pragma once
#include "GL.hpp"
#include "Load.hpp"
#include <vector>

struct Texture {
    static GLuint load_from_png(std::string const &filename);
};

extern Load< std::vector< GLuint > > textures;