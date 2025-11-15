#include "Textures.hpp"

#include "load_save_png.hpp"
#include <glm/glm.hpp>
#include <iostream>

GLuint Texture::load_from_png(std::string const &filename) {
    GLuint tex = 0;
    glGenTextures(1, &tex);

    if (tex == 0) {
        std::cerr << "WARNING: Failed to generate texture buffer. Skipping texture " << filename << std::endl;
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, tex);

    std::vector< glm::u8vec4 > pixels;
    glm::uvec2 size;
    
    load_png(filename, &size, &pixels, OriginLocation::LowerLeftOrigin);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, (GLsizei)size.x, (GLsizei)size.y,
        0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}