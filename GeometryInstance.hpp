#pragma once

#include "Mesh.hpp"
#include "Scene.hpp"

struct Instancer {
    Scene::Drawable::Pipeline pipeline;
    
    std::vector< glm::mat4 > world_mats;

    GLuint vbo_vert = 0; // the buffer where the provided mesh exists
    GLuint vbo_world = 0; // clip space position of each instance, generated 
                         // by this object

    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::u8vec4 Color;
        glm::vec2 TexCoord;
    };
    static_assert(sizeof(Vertex) == 36);

    void make_vao();
    void draw(glm::mat4 world_to_clip, glm::mat4x3 world_to_light);
};