#include "Particles.hpp"
#include <iostream>

void ParticleGenerator::continuous_update(float elapsed) {
    spawn_timer += elapsed;
    int can_spawn = (int)(spawn_timer / SPAWN_RATE);
    
    for (size_t i = 0; i < MAX_PARTICLES; ++i) {
        auto &p = vertices[i];
        auto &s = state[i];

        if (s.time <= 0.f && can_spawn) {
            --can_spawn;
            s.time = LIFETIME;
            p.Size = SIZE;
            
            glm::vec3 sampled = sampler(rng);
            p.Position = transform.make_world_from_local()[3]; // * glm::vec4(sampled, 1.f);
            s.velocity = SPEED * glm::normalize(sampled);
            spawn_timer = 0.f;
        }
        else if (s.time > 0.f) {
            s.time -= elapsed;
            p.Position += s.velocity * elapsed; 
            
        }
    }

    std::vector< ParticleInstanceBuffer::Vertex > alive;
    for (size_t i = 0; i < MAX_PARTICLES; ++i) {
        if (state[i].time > 0.f) {
            alive.push_back(vertices[i]);
        }
    }

    if (!alive.empty())
        particles.set(alive, GL_DYNAMIC_DRAW);
}

void ParticleGenerator::burst_at(glm::vec3 position, size_t particle_count) {
    for (size_t i = 0; i < MAX_PARTICLES; ++i) {
        auto &p = vertices[i];
        auto &s = state[i];

        if (particle_count <= 0) break;

        if (s.time <= 0.f) {
            s.time = LIFETIME;
            p.Size = SIZE;

            p.Position = position;
            s.velocity = SPEED * glm::normalize(sampler(rng));
            
            --particle_count;
        }
    }
};


#include "gl_errors.hpp"

#include <set>
#include <cassert>

// modified dynamic mesh buffer
ParticleInstanceBuffer::ParticleInstanceBuffer() {
	glGenBuffers(1, &buffer);
	//Now that we have a buffer name, need to bind it to actually create the buffer object:
	//"No buffer objects are associated with the returned buffer object names until they are first bound by calling glBindBuffer." (https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGenBuffers.xhtml)
	glBindBuffer(GL_ARRAY_BUFFER, buffer); 
	// (but don't have to do anything with it while it is bound)
	glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &corner_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, corner_buffer);

    // bind corners for instancing
    std::vector < glm::vec2 > corners = {
        {-.5f, -.5f},
        {.5f, -.5f},
        {-.5f, .5f},
        {.5f, .5f}
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * corners.size(), corners.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

	GL_ERRORS();
}

ParticleInstanceBuffer::~ParticleInstanceBuffer() {
	glDeleteBuffers(1, &buffer);
	glDeleteBuffers(1, &corner_buffer);
	buffer = 0;
    corner_buffer = 0;
    count = 0;

	GL_ERRORS();
}

void ParticleInstanceBuffer::set(Vertex const *data, size_t count_, GLenum usage) {
	//store the count for later:
	count = (GLuint)count_;

	glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vertex), data, usage);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GL_ERRORS();
}

void ParticleInstanceBuffer::draw() {
    if (vao == 0) {
        make_vao_for_program(particle_program->program);
    }


    glUseProgram(particle_program->program);
    glBindVertexArray(vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, particle_program_pipeline.textures[0].texture);

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

void ParticleInstanceBuffer::make_vao_for_program(GLuint program) {
	//look up each attribute location in the program and bind it to the buffer with the correct offset and stride:
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, buffer);

	//keep track of which attributes are bound for debugging purposes:
	std::set< GLuint > bound;

	{ //Bind "Position" if it exists:
		GLint location = glGetAttribLocation(program, "Position");
		if (location != -1) {
			glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Position));
			glEnableVertexAttribArray(location);
            glVertexAttribDivisor(location, 1);
			bound.insert(location);
		}
        assert(location > -1);
	}
	{ //Bind "Size" if it exists:
		GLint location = glGetAttribLocation(program, "Size");
		if (location != -1) {
			glVertexAttribPointer(location, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Size));
			glEnableVertexAttribArray(location);
            glVertexAttribDivisor(location, 1);
			bound.insert(location);
		}
        assert(location > -1);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, corner_buffer);

	{ //Bind "Corner" if it exists:
		GLint location = glGetAttribLocation(program, "Corner");
		if (location != -1) {
			glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, 0, (GLbyte *)0);
			glEnableVertexAttribArray(location);
            glVertexAttribDivisor(location, 0);
			bound.insert(location);
		}

        assert(location > -1);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	//Check that all active attributes were bound:
	GLint active = 0;
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &active);
	assert(active >= 0 && "Doesn't makes sense to have negative active attributes.");
	for (GLuint i = 0; i < GLuint(active); ++i) {
		GLchar name[100];
		GLint size = 0;
		GLenum type = 0;
		glGetActiveAttrib(program, i, 100, NULL, &size, &type, name);
		name[99] = '\0';
		GLint location = glGetAttribLocation(program, name);
		if (!bound.count(GLuint(location))) {
			throw std::runtime_error("ERROR: active attribute '" + std::string(name) + "' in program is not bound.");
		}
	}

	GL_ERRORS();
}
