#pragma once

#include "glm/glm.hpp"
#include "GL.hpp"

#include <map>

#include "Load.hpp"

struct OverlayProgram {
    OverlayProgram();
    ~OverlayProgram();

    GLuint program = 0;

    // the initial screen size to scale ui elements upon
    // window size changes 
    GLint ORIG_SCREEN_SIZE_vec2 = -1U;
    GLint SCREEN_SIZE_vec2 = -1U;
    GLint ORIG_ASPECT_RATIO_float = -1U;
    GLint POSITION_vec2 = -1U;
    GLint SIZE_vec2 = -1U;
    GLint COLOR_vec4 = -1U;

	//Textures:
	//TEXTURE0 - texture of the UI element to draw

    GLuint empty_vao = 0; 
};

extern Load < OverlayProgram > overlay_program;

// overlays have origin (0,0) at the center of the screen.
// valid points [-width // 2, width // 2] x [-height // 2, height // 2]
struct Overlay {
    // --- overlay setup
    Overlay(); 

    glm::uvec2 size = glm::uvec2(0);

    GLuint fb = 0;
    GLuint tex = 0;

    // --- overlay elements
    struct Element {
        glm::vec2 position; // center of rectangle
        glm::vec2 size; // width, height
        glm::vec4 color;
        GLuint tex = 0;
        bool visible = true;
    };

    void resize(glm::uvec2 const &drawable_size);
    void add_element(std::string name, glm::vec2 position, glm::vec2 size, glm::vec4 color, GLuint tex);
    void add_element(std::string name, Element element);
    void remove_element(std::string name);

    std::map< std::string, Element > elements;

    void draw(); 

    // --- overlay interaction
    struct Interaction {
        Element &element;
        std::function< void() > action;
        Interaction(Element &element, std::function< void() > action) : element(element), action(action) { };
    };

    std::map< std::string, Interaction > interactions;

    void add_interaction(std::string name, std::function< void() > action);
    void remove_interaction(std::string name);

    void handle_click(glm::uvec2 click_position);

    private:
        glm::uvec2 initial_size = glm::uvec2(0);
};