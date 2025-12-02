#include "Overlay.hpp"

#include "gl_compile_program.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "gl_errors.hpp"
#include "check_fb.hpp"

Load< OverlayProgram > overlay_program(LoadTagEarly, []() -> OverlayProgram const * {
    return new OverlayProgram();
});

OverlayProgram::OverlayProgram() {
    program = gl_compile_program(
        "#version 330\n"
        "uniform vec2 ORIG_SCREEN_SIZE;\n"
        "uniform vec2 SCREEN_SIZE;\n"
        "uniform vec2 POSITION;\n"
        "uniform vec2 SIZE;\n"
        "uniform vec4 COLOR;\n"
        "out vec4 color;\n"
        "out vec2 texCoord;\n"
        "void main() {\n"
            "const vec2 corners[4] = vec2[](\n" // avoid vao
                "vec2(-1.0, -1.0),\n"
                "vec2(1.0, -1.0),\n"
                "vec2(-1.0, 1.0),\n"
                "vec2(1.0, 1.0)\n"
            ");\n"
            "float aspect = SCREEN_SIZE.x / SCREEN_SIZE.y;\n"
            "float orig_aspect = ORIG_SCREEN_SIZE.x / ORIG_SCREEN_SIZE.y;\n"
            "float scale = (aspect > orig_aspect) ?\n"
                "SCREEN_SIZE.y / ORIG_SCREEN_SIZE.y :\n"
                "SCREEN_SIZE.x / ORIG_SCREEN_SIZE.x;\n"
            "vec2 ndc = (POSITION + corners[gl_VertexID] * SIZE * scale) / SCREEN_SIZE;\n"
            "gl_Position = vec4(ndc, 0.0, 1.0);\n"
            "color = COLOR;\n"
            "texCoord = corners[gl_VertexID] * 0.5 + 0.5;\n"
        "}\n"
    ,
        "#version 330\n"
        "uniform sampler2D TEX;\n"
        "in vec4 color;\n"
        "in vec2 texCoord;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "   fragColor = texture(TEX, texCoord) * color;\n"
        "}\n"
    );

    ORIG_SCREEN_SIZE_vec2 = glGetUniformLocation(program, "ORIG_SCREEN_SIZE");
    SCREEN_SIZE_vec2 = glGetUniformLocation(program, "SCREEN_SIZE");
    ORIG_ASPECT_RATIO_float = glGetUniformLocation(program, "ORIG_ASPECT_RATIO");
    POSITION_vec2 = glGetUniformLocation(program, "POSITION");
    SIZE_vec2 = glGetUniformLocation(program, "SIZE");
    COLOR_vec4 = glGetUniformLocation(program, "COLOR");

    GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

    glUseProgram(program);

    glUniform1i(TEX_sampler2D, 0);
    
    glUseProgram(0);

    glGenVertexArrays(1, &empty_vao);
}

OverlayProgram::~OverlayProgram() {
    glDeleteProgram(program);
    program = 0;
}

Overlay::Overlay() {
    // set initial screen size
    GLint viewport[4]; 
    glGetIntegerv(GL_VIEWPORT, &viewport[0]);

    initial_size = glm::uvec2(viewport[2], viewport[3]);

    glUseProgram(overlay_program->program);
    glUniform2fv(overlay_program->SCREEN_SIZE_vec2, 1,
        glm::value_ptr(glm::vec2(initial_size)));
    glUniform2fv(overlay_program->ORIG_SCREEN_SIZE_vec2, 1,
        glm::value_ptr(glm::vec2(initial_size)));
    glUseProgram(0);
}
void Overlay::resize(glm::uvec2 const &size) {
    if (this->size == size) return;
    this->size = size;

    if (tex == 0) {
        glGenTextures(1, &tex);
    }

    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (fb == 0) {
        glGenFramebuffers(1, &fb);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        GLenum bufs[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, bufs);
        check_fb();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    GL_ERRORS();
};

void Overlay::draw() {
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glClearColor(0.f, 0.f, 0.f, 0.0f); 
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, (GLsizei)size.x, (GLsizei)size.y);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(overlay_program->program);
    glBindVertexArray(overlay_program->empty_vao);
    glUniform2fv(overlay_program->SCREEN_SIZE_vec2, 1, glm::value_ptr(glm::vec2(size)));
    for (auto elem : elements) {
        glUniform2fv(overlay_program->POSITION_vec2, 1, glm::value_ptr(elem.second.position));
        glUniform4fv(overlay_program->COLOR_vec4, 1, glm::value_ptr(elem.second.color));
        glUniform2fv(overlay_program->SIZE_vec2, 1, glm::value_ptr(elem.second.size));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, elem.second.tex);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Overlay::add_element(std::string name, glm::vec2 position, glm::vec2 elem_size, glm::vec4 color, GLuint elem_tex) {
    Element element;
    element.position = position;
    element.size = elem_size;
    element.color = color;
    element.tex = elem_tex;
    elements.insert({ name, element });
}
void Overlay::add_element(std::string name, Element element) {
    elements.insert({ name, element });
}

void Overlay::remove_element(std::string name) {
    elements.erase(name);
}

void Overlay::add_interaction(std::string name, std::function< void() > action) {
    auto it = elements.find(name);
    if (it == elements.end()) {
        std::cerr << "WARNING: element with name " << name << " does not exist in overlay!" << std::endl;
        return;
    }

    interactions.insert({ name, Interaction(it->second, action) });
}

void Overlay::remove_interaction(std::string name) {
    interactions.erase(name);
};

void Overlay::handle_click(glm::uvec2 click_position) {
    float aspect = size.x / size.y;
    float orig_aspect = initial_size.x / initial_size.y;
    float scale = (glm::vec2(size) / glm::vec2(initial_size))[(size_t)(aspect > orig_aspect)];

    for (auto p : interactions) {
        Element &e = p.second.element;
        if (glm::abs((float)click_position.x - e.position.x - (float)size.x * .5f) <= e.size.x * .5f * scale &&
            glm::abs((float)click_position.y - e.position.y - (float)size.y * .5f) <= e.size.y * .5f * scale
        ) {
            p.second.action();
        }
        break;
    }
}