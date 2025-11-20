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
    // General
    {"Steel", "textures/steel.png"},
    {"Brown Metal", "textures/brown_metal.png"},
    {"Rubber", "textures/rubber.png"},
    // Objects
    {"Fence Brick", "textures/BrickWall001a_Color.png"},
    {"Street lamp", "textures/old lamp post_BaseColor.png"},
    {"Cobblestone road", "textures/road_C_fix.png"},
    {"Park Bench", "textures/Park Bench_BaseColor.1001.png"},
    {"Fence_array", "textures/steel.png"},
    {"preset_0", "textures/modular_electricity_poles_diff_8k.png"},
    // {"Ground", "textures/compitition1_Branch_BaseColor.png"},
    // Animals
    {"Tyriese_Miller_C_Deer_Idle:body", "textures/deer_skin_light.png"},
    {"Tyriese_Miller_C_Deer_Idle:antler", "textures/bone.png"},
    {"Deer Behind Fence", "textures/deer_skin_plain.png"},
    {"Dead Deer", "textures/deer_skin_spotted.png"},
    {"Rat", "textures/rat.png"},
    {"Ground", "textures/grass.png"},
    {"177d920ddde57c74f8e1ef18863ab511", "textures/sign.png"},
    {"Zoo Sign.001", "textures/zoo_sign_1.png"},
    {"Zoo Sign.002", "textures/zoo_sign_2.png"},
    // Big House
    {"Roof", "textures/compitition1_Chimini_BaseColor.png"},
    {"Door", "textures/compitition1_Door_BaseColor.png"},
    {"Wood", "textures/wood.png"},
    {"Flower Pot", "textures/compitition1_Pot_BaseColor.png"},
    {"NurbsPath", "textures/compitition1_Branch_BaseColor.png"},
    {"Flower", "textures/compitition1_Flower1_BaseColor.png"},
    {"Rope", "textures/compitition1_Rope_BaseColor.png"},
    {"Stairs.", "textures/compitition1_Stairs_BaseColor.png"},
    {"Stairs Support", "textures/compitition1_Stairs Support_BaseColor.png"},
    {"Structure", "textures/compitition1_Structure_BaseColor.png"},
    {"Terras", "textures/compitition1_Terras_BaseColor.png"},
    {"House", "textures/compitition1_House_BaseColor.png"},
    {"Chair", "textures/compitition1_Chair_BaseColor.png"},
    // Nature
    {"Low Poly Evergreen Tree", "textures/evergreen.png"},
    {"Mossy Rock 1", "textures/rock_1.png"},
    {"Mossy Rock 2", "textures/rock_2.png"},
    {"Wild Grass", "textures/Grass_basecolor.png"},
    // Cars
    {"licensePlate", "textures/license plate.png"},
    {"Volvo body 1", "textures/blue_metal.png"},
    {"Volvo body 2", "textures/red_metal.png"},
    {"rim", "textures/rubber.png"},
    {"tire", "textures/rubber.png"},
    {"wind", "textures/window.png"},
    // Small house
    {"Small House Terra", "textures/small_house_terra.png"},
    {"Small House Main", "textures/small_house_main.png"},
    {"Small House Tower", "textures/small_house_tower_2.png"},
};

Load<std::vector<GLuint>> textures(LoadTagDefault, []() -> std::vector<GLuint> const *
                                   {
    auto ret = new std::vector<GLuint>();
    ret->reserve(named_textures.size());

    for (auto const &nt : named_textures) {
        ret->emplace_back(Texture::load_from_png(data_path(nt.filename)));
    }

    return ret; });