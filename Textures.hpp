#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include <string>
#include <vector>
#include <map>

struct Texture
{
    static GLuint load_from_png(std::string const &filename);
};

struct NamedTexture
{
    std::string prefix;   // transform name prefix, e.g. "Fence"
    std::string filename; // data_path-relative PNG path
};

extern const std::vector<NamedTexture> named_textures;

extern Load<std::vector<GLuint>> textures;
extern Load<std::map<std::string, GLuint>> ui_textures;