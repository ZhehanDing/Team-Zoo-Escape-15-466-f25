#pragma once

#include "GL.hpp"
#include <string>

namespace Texture {
    GLuint load_from_png(std::string const &filename);
};