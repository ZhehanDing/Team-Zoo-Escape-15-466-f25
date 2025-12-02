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
    {"light Metal", "textures/light_metal.png"},
    {"Rubber", "textures/rubber.png"},
    // Objects
    {"Fence Brick", "textures/BrickWall001a_Color.png"},
    {"Street lamp", "textures/old lamp post_BaseColor.png"},
    {"Окружность", "textures/steel.png"},
    {"Цилиндр", "textures/steel.png"},
    {"Cobblestone road", "textures/road_C_fix.png"},
    {"Park Bench", "textures/Park Bench_BaseColor.1001.png"},
    {"Fence_array", "textures/steel.png"},
    {"preset_0", "textures/modular_electricity_poles_diff_8k.png"},
    // {"Ground", "textures/compitition1_Branch_BaseColor.png"},
    // Animals
    {"Tyriese_Miller_C_Deer_Idle:body", "textures/deer_skin_light.png"},
    {"Tyriese_Miller_C_Deer_Idle:antler", "textures/bone.png"},
    {"HG_Baggy_Jeans", "textures/Dwain.001_HG_Baggy_Jeans.002_base color.png"},
    {"HG_Suede_Sneakers_Male", "textures/Dwain.001_HG_Suede_Sneakers_Male.002_base color.png"},
    {"Deer Behind Fence", "textures/deer_skin_plain.png"},
    {"Dead Deer", "textures/deer_skin_spotted.png"},
    {"Rat", "textures/rat.png"},
    {"Ground", "textures/grass_blue.png"},
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
    {"lincensePlate", "textures/license plate.png"},
    {"Volvo body 1", "textures/blue_metal.png"},
    {"Volvo body 2", "textures/red_metal.png"},
    {"rim", "textures/rubber.png"},
    {"tire", "textures/rubber.png"},
    {"wind", "textures/window.png"},
    // Small house
    {"Small House Terra", "textures/small_house_terra.png"},
    {"Small House Main", "textures/small_house_main.png"},
    {"Small House Tower", "textures/small_house_tower_2.png"},
    // Statues
    {"Angry Bear", "textures/angry-bear.png"},
    {"Bear Sculpture", "textures/BearStatue.png"},
    {"Fish Statue", "textures/rock_1.png"},
    {"Terracotta owl", "textures/Owl_Color.png"},
    // Enemy textures
    {"base", "textures/base.png"},
    // Civilian
    {"base_00", "textures/base_00.png"},
    {"base_01", "textures/base_01.png"},
    {"base_02", "textures/base_02.png"},
    {"base_03", "textures/base_03.png"},
    {"base_04", "textures/base_04.png"},
    {"base_05", "textures/base_05.png"},
    {"base_06", "textures/base_06.png"},
    {"base_07", "textures/base_07.png"},
    {"base_08", "textures/base_08.png"},
    {"base_09", "textures/base_09.png"},
    {"eyebrow011", "textures/eyebrow011.png"},
    {"eyelashes01", "textures/eyelashes01.png"},
    {"fedora_cocked", "textures/fedora_cocked.png"},
    {"male_elegantsuit01", "textures/male_elegantsuit01.png"},
    {"tennisshoes", "textures/tennisshoes.png"},
    {"teeth_shape03", "textures/teeth_shape03.png"},
    {"boots_ankle_male", "textures/boots_ankle_male.png"},
    {"tongue01", "textures/tongue01.png"},
    // Shared
    {"eyebrow001", "textures/eyebrow001.png"},
    {"eyebrow003", "textures/eyebrow003.png"},
    {"eyebrow004", "textures/eyebrow004.png"},
    {"eyelashes03", "textures/eyelashes03.png"},
    {"teeth_base", "textures/teeth_base.png"},
    {"teeth_shape01", "textures/teeth_shape01.png"},
    // Clothing
    {"male_casualsuit01", "textures/male_casualsuit01.png"},
    {"male_casualsuit03", "textures/male_casualsuit03.png"},
    {"male_casualsuit05", "textures/male_casualsuit05.png"},
    {"male_casualsuit06", "textures/male_casualsuit06.png"},
    {"male_worksuit01", "textures/male_worksuit01.png"},
    {"crude_male_shirt", "textures/crude_male_shirt.png"},
    {"Polo_t-shirt", "textures/Polo_t-shirt.png"},
    {"sweater_fisherman", "textures/sweater_fisherman.png"},
    {"short02", "textures/short02.png"},
    {"short_messy", "textures/short_messy.png"},
    {"tightjeans", "textures/tightjeans.png"},
    {"elvs_disco_pant1", "textures/elvs_disco_pant1.png"},
    {"elvs_gored_elephantpant1", "textures/elvs_gored_elephantpant1.png"},
    {"f_trousers_01", "textures/f_trousers_01.png"},
    {"m_trousers_01", "textures/m_trousers_01.png"},
    {"o4saken_long01", "textures/o4saken_long01.png"},
    // Shoes
    {"shoes_monk_strap_female", "textures/shoes_monk_strap_female.png"},
    {"shoes_oxford_female", "textures/shoes_oxford_female.png"},
    {"sneakers", "textures/sneakers.png"},
    {"dudoc_balletflat1", "textures/dudoc_balletflat1.png"},
    {"dudoc_domsjeans1", "textures/dudoc_domsjeans1.png"},
    {"harvey_shoesaddle1", "textures/harvey_shoesaddle1.png"},
    {"hero_boots_3", "textures/hero_boots_3.png"},
    // Hair
    {"afro01", "textures/afro01.png"},
    {"hair_05", "textures/hair_05.png"},
    {"mhair02", "textures/mhair02.png"},
    {"bob_inverted_bangs", "textures/bob_inverted_bangs.png"},
    {"littleright_hair_bobcut", "textures/littleright_hair_bobcut.png"},
    {"ponytail01", "textures/ponytail01.png"},
    // Deer Human textures
    {"elvs_antlers1", "textures/elvs_antlers1.png"},
    {"mens_trouser_short_nc_elvcrf", "textures/mens_trouser_short_nc_elvcrf.png"},
    {"eyebrow002", "textures/eyebrow002.png"},
    {"eyelashes01", "textures/eyelashes01.png"},
    {"heroine_boots_1", "textures/heroine_boots_1.png"},
};

Load<std::vector<GLuint>> textures(LoadTagDefault, []() -> std::vector<GLuint> const *
                                   {
    auto ret = new std::vector<GLuint>();
    ret->reserve(named_textures.size());

    for (auto const &nt : named_textures) {
        ret->emplace_back(Texture::load_from_png(data_path(nt.filename)));
    }

    return ret; });


const std::vector< NamedTexture > ui_named_textures = {
    {"Stalk Tooltip", "textures/stalk_tooltip.png"},
    {"Play Button", "textures/play_button.png"}
};

Load<std::map<std::string, GLuint>> ui_textures(LoadTagDefault, []() -> std::map<std::string, GLuint> const * {
    auto ret = new std::map<std::string, GLuint>();
    for (auto const &nt : ui_named_textures) {
        ret->insert({nt.prefix, Texture::load_from_png(data_path(nt.filename))});
    }
    return ret; 
});