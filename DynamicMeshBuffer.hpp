#pragma once

#include "GL.hpp"

#include <glm/glm.hpp>

#include <vector>

struct DynamicMeshBuffer {
	//DynamicMeshBuffer manages dynamically-uploaded vertices.

	//the vertices are stored in this vertex buffer:
	GLuint buffer = 0;

	//there are this many vertices in the buffer: (as of the last 'set()' call)
	GLuint count = 0;

	//the vertices have this format:
	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::u8vec4 Color;
		glm::vec2 TexCoord;
	};
	static_assert(sizeof(Vertex) == 4*3 + 4*3 + 4*1 + 4*2, "Vertex structure is packed.");

	//you can set the contents of the buffer using this function:
	// (set 'usage' based on how frequently you plan to upload and what you plan to do with the data GL_STREAM_DRAW is probably what you want.)
	void set(Vertex const *data, size_t count, GLenum usage);
	void set(std::vector< Vertex > const &data, GLenum usage) {
		set(data.data(), data.size(), usage);
	}

	//make a vertex array object describing how to map this buffer to the attributes in a given program:
	// (program should have "Position", "Normal", "Color", and "TexCoord" attributes.)
	//return: a new vertex array object. (caller responsible for freeing when done)
	GLuint make_vao_for_program(GLuint program) const;

	//----------------

	//allocate and clean up the buffer name:
	DynamicMeshBuffer();
	~DynamicMeshBuffer();
};