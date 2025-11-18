#include "Textures.hpp"

#include "load_save_png.hpp"
#include "data_path.hpp"
#include <glm/glm.hpp>
#include <iostream>

GLuint Texture::load_from_png(std::string const &filename)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);

    if (tex == 0)
    {
        std::cerr << "WARNING: Failed to generate texture buffer. Skipping texture " << filename << std::endl;
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, tex);

    std::vector<glm::u8vec4> pixels;
    glm::uvec2 size;

    load_png(filename, &size, &pixels, OriginLocation::LowerLeftOrigin);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8,
                 (GLsizei)size.x, (GLsizei)size.y,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

const std::vector<NamedTexture> named_textures = {
    {"Fence Brick", "textures/BrickWall001a_Color.png"},
    {"Street lamp", "textures/old lamp post_BaseColor.png"},
    {"Cobblestone road", "textures/road_C_fix.png"},
    {"Park Bench", "textures/Park Bench_BaseColor.1001.png"},
    {"Low Poly Evergreen Tree", "textures/evergreen.png"},
    {"Fence_array", "textures/steel.png"},
    {"Steel", "textures/steel.png"},
    {"Tyriese_Miller_C_Deer_Idle:body", "textures/deer_skin_plain.png"},
    {"Tyriese_Miller_C_Deer_Idle:antler", "textures/bone.png"},
    {"Deer Behind Fence", "textures/deer_skin_plain.png"},
    // {"Ground", "textures/grass.png"},
    {"177d920ddde57c74f8e1ef18863ab511", "textures/sign.png"},
};

Load<std::vector<GLuint>> textures(LoadTagDefault, []() -> std::vector<GLuint> const *
                                   {
    auto ret = new std::vector<GLuint>();
    ret->reserve(named_textures.size());

    for (auto const &nt : named_textures) {
        ret->emplace_back(Texture::load_from_png(data_path(nt.filename)));
    }

    return ret; });