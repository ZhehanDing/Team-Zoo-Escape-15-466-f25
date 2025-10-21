#include "DynamicMeshBuffer.hpp"

#include "gl_errors.hpp"

#include <set>
#include <cassert>

DynamicMeshBuffer::DynamicMeshBuffer() {
	glGenBuffers(1, &buffer);
	//Now that we have a buffer name, need to bind it to actually create the buffer object:
	//"No buffer objects are associated with the returned buffer object names until they are first bound by calling glBindBuffer." (https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGenBuffers.xhtml)
	glBindBuffer(GL_ARRAY_BUFFER, buffer); 
	// (but don't have to do anything with it while it is bound)
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GL_ERRORS();
}

DynamicMeshBuffer::~DynamicMeshBuffer() {
	glDeleteBuffers(1, &buffer);
	buffer = 0;
	count = 0;

	GL_ERRORS();
}

void DynamicMeshBuffer::set(DynamicMeshBuffer::Vertex const *data, size_t count_, GLenum usage) {
	//store the count for later:
	count = (GLuint)count_;

	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vertex), data, usage);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GL_ERRORS();
}

GLuint DynamicMeshBuffer::make_vao_for_program(GLuint program) const {
	//look up each attribute location in the program and bind it to the buffer with the correct offset and stride:
	GLuint vao = 0;
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
			bound.insert(location);
		}
	}
	{ //Bind "Normal" if it exists:
		GLint location = glGetAttribLocation(program, "Normal");
		if (location != -1) {
			glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Normal));
			glEnableVertexAttribArray(location);
			bound.insert(location);
		}
	}
	{ //Bind "Color" if it exists:
		GLint location = glGetAttribLocation(program, "Color");
		if (location != -1) {
			glVertexAttribPointer(location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Color));
			glEnableVertexAttribArray(location);
			bound.insert(location);
		}
	}
	{ //Bind "TexCoord" if it exists:
		GLint location = glGetAttribLocation(program, "TexCoord");
		if (location != -1) {
			glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, TexCoord));
			glEnableVertexAttribArray(location);
			bound.insert(location);
		}
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

	return vao;
}