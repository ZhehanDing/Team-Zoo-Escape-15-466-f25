#include "GeometryInstance.hpp"
#include "BasicMaterialDeferredInstancingProgram.hpp"
#include <glm/gtc/type_ptr.hpp>
#include "gl_errors.hpp"
#include <set>

void Instancer::make_vao_for_program(GLuint program) {
    assert(vbo_vert != 0 && "Invalid vertex buffer!");
    if (vbo_world == 0) {
        glGenBuffers(1, &vbo_world);
    }

    pipeline.program = program;
    glBindBuffer(GL_ARRAY_BUFFER, vbo_world);
    glBufferData(GL_ARRAY_BUFFER, world_mats.size() * sizeof(glm::mat4), 
        world_mats.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    assert(vbo_world);

    if (pipeline.vao == 0)
	    glGenVertexArrays(1, &pipeline.vao);
	glBindVertexArray(pipeline.vao);

    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_vert);
        GLint location = glGetAttribLocation(pipeline.program, "Position");
        if (location != -1) {
            glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *) 0 + offsetof(Vertex, Position));
            glEnableVertexAttribArray(location);
            glVertexAttribDivisor(location, 0);
        }
        location = glGetAttribLocation(pipeline.program, "Normal");
        if (location != -1) {
            glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *) 0 + offsetof(Vertex, Normal));
            glEnableVertexAttribArray(location);
            glVertexAttribDivisor(location, 0);
        }
        location = glGetAttribLocation(pipeline.program, "Color");
        if (location != -1) {
            glVertexAttribPointer(location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *) 0 + offsetof(Vertex, Color));
            glEnableVertexAttribArray(location);
            glVertexAttribDivisor(location, 0);
        }
        location = glGetAttribLocation(pipeline.program, "TexCoord");
        if (location != -1) {
            glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *) 0 + offsetof(Vertex, TexCoord));
            glEnableVertexAttribArray(location);
            glVertexAttribDivisor(location, 0);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_world);
        GLint location = glGetAttribLocation(pipeline.program, "WorldFromLocal");
        if (location != -1) {
            for (GLuint i = 0; i < 4; ++i) {
                glVertexAttribPointer(location + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (GLbyte *) 0 + sizeof(float) * 4 * i);
                glEnableVertexAttribArray(location + i);
                glVertexAttribDivisor(location + i, 1);
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    glBindVertexArray(0);
    GL_ERRORS();
}

void Instancer::draw(glm::mat4 world_to_clip, glm::mat4x3 world_to_light) {
    if (pipeline.program == 0 || pipeline.vao == 0 || pipeline.count == 0) return;

    glUseProgram(pipeline.program);
    glBindVertexArray(pipeline.vao);

    glUniformMatrix4fv(basic_material_deferred_object_instancing_program->WORLD_TO_CLIP_mat4, 
        1, GL_FALSE, glm::value_ptr(world_to_clip));
    glUniformMatrix4x3fv(basic_material_deferred_object_instancing_program->WORLD_TO_LIGHT_mat4x3, 
        1, GL_FALSE, glm::value_ptr(world_to_light));
    
    if (pipeline.set_uniforms) pipeline.set_uniforms();
    
    for (uint32_t i = 0; i < Scene::Drawable::Pipeline::TextureCount; ++i) {
        if (pipeline.textures[i].texture != 0) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(pipeline.textures[i].target, pipeline.textures[i].texture);
        }
    }

    glDrawArraysInstanced(pipeline.type, pipeline.start, pipeline.count, (GLsizei)world_mats.size());

    for (uint32_t i = 0; i < Scene::Drawable::Pipeline::TextureCount; ++i) {
        if (pipeline.textures[i].texture != 0) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(pipeline.textures[i].target, 0);
        }
    }
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(0);
    glUseProgram(0);
    GL_ERRORS();
}
