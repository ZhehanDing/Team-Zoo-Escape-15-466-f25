#pragma once

#include "GL.hpp"

#include "Scene.hpp"
#include "ParticleProgram.hpp"

#include <glm/glm.hpp>
#include <functional>
#include <vector>
#include <random>

// Renamed DynamicMeshBuffer, and modified for instancing.
struct ParticleInstanceBuffer {
	//DynamicMeshBuffer manages dynamically-uploaded vertices.

	//the vertices are stored in this vertex buffer:
	GLuint vao = 0;
	GLuint buffer = 0;
    GLuint corner_buffer = 0; // 4 corners are here for instancing

	//there are this many vertices in the buffer: (as of the last 'set()' call)
	GLuint count = 0;

	//the vertices have this format:
	struct Vertex {
		glm::vec3 Position;
		float Size;
	};
	static_assert(sizeof(Vertex) == 4*3 + 4*1, "Vertex structure is packed.");

	//you can set the contents of the buffer using this function:
	// (set 'usage' based on how frequently you plan to upload and what you plan to do with the data GL_STREAM_DRAW is probably what you want.)
	void set(Vertex const *data, size_t count, GLenum usage);
	void set(std::vector< Vertex > const &data, GLenum usage) {
		set(data.data(), data.size(), usage);
	}

    void draw();

	//make a vertex array object describing how to map this buffer to the attributes in a given program:
	// (program should have "Position", "Corner", "Size", and "LookAt" attributes.)
	//return: a new vertex array object. (caller responsible for freeing when done)
	void make_vao_for_program(GLuint program);

	//----------------

	//allocate and clean up the buffer name:
	ParticleInstanceBuffer();
	~ParticleInstanceBuffer();
};

struct ParticleState {
    float time = 0.f;
    glm::vec3 velocity;
};

struct ParticleGenerator {
    Scene::Transform transform;

    size_t MAX_PARTICLES = 100;
    float LIFETIME = 2.f;
    float SPAWN_RATE = 1.f; // 1 particle every SPAWN_RATE seconds
    float SIZE = .5f;
    float SPEED = 2.f;

    enum Type { // have yet to decide data setup for LOCAL computation, likely will have transform on ParticleGenerator as "center"
        LOCAL, GLOBAL
    } type = GLOBAL;

    void set_lifetime(float time) { LIFETIME = time; };
    void set_max_particles(size_t max) { 
        MAX_PARTICLES = max; 
        vertices.resize(MAX_PARTICLES); 
        state.resize(MAX_PARTICLES);
    };
    void set_spawn_rate(float rate) { SPAWN_RATE = rate; };
    void set_size(float size) { SIZE = size; };
    void set_speed(float speed) { SPEED = speed; };

    void continuous_update(float elapsed);
    void burst_at(glm::vec3 position, size_t particle_count);


    // sampler of position where to spawn particle
    ParticleGenerator(std::function< glm::vec3(std::mt19937 &) > sampler)
    : sampler(sampler) { 
        vertices.resize(MAX_PARTICLES); 
        state.resize(MAX_PARTICLES);
    };

    void draw() { particles.draw(); };

    private:
        float spawn_timer = 0.f;
        std::function< glm::vec3(std::mt19937 &) > sampler;
        std::vector< ParticleInstanceBuffer::Vertex > vertices;
        std::vector< ParticleState > state;

        ParticleInstanceBuffer particles;
        std::mt19937 rng{};
};