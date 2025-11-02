#include "RiggedMesh.hpp"

#include "read_write_chunk.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "gl_errors.hpp"

#include <stdexcept>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <cstddef>

BoneInfluenceBuffer::BoneInfluenceBuffer(std::string const &filename) {
	glGenBuffers(1, &buffer);

	std::ifstream file(filename, std::ios::binary);

	std::vector < BoneInfluence > infl_data;
	//read + upload data chunk:
	if (filename.size() >= 5 && filename.substr(filename.size()-5) == ".infl") {
		read_chunk(file, "infl", &infl_data);

		// per Mesh.cpp
		//upload data:
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, infl_data.size() * sizeof(BoneInfluence), infl_data.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		total = GLuint(infl_data.size()); //store total for later checks on index

		BoneIndices = MeshBuffer::Attrib(4, GL_UNSIGNED_INT, GL_FALSE, sizeof(BoneInfluence), offsetof(BoneInfluence, BoneIndices));
		BoneWeights = MeshBuffer::Attrib(4, GL_FLOAT, GL_FALSE, sizeof(BoneInfluence), offsetof(BoneInfluence, BoneWeights));
	
	} else {
		throw std::runtime_error("Unknown file type '" + filename + "'");
	}

	std::vector< char > strings;
	read_chunk(file, "str0", &strings);

	{ //read index chunk, add to meshes:
		struct IndexEntry {
			uint32_t name_begin, name_end;
			uint32_t vertex_begin, vertex_end;
		};
		static_assert(sizeof(IndexEntry) == 16, "Index entry should be packed");

		std::vector< IndexEntry > index;
		read_chunk(file, "idx0", &index);

		/*
		struct Matrices {
			glm::mat4 mesh_from_world;
			glm::mat4 world_from_mesh;
		};
		static_assert(sizeof(Matrices) == 128, "Matrices data should be packed");
		
		std::vector< Matrices > mats;
		read_chunk(file, "mat0", &mats);
		*/
		/*
		for (size_t i = 0; i < index.size(); ++i) {
			auto const &entry = index[i];
			if (!(entry.name_begin <= entry.name_end && entry.name_end <= strings.size())) {
				throw std::runtime_error("index entry has out-of-range name begin/end");
			}
			if (!(entry.vertex_begin <= entry.vertex_end && entry.vertex_end <= total)) {
				throw std::runtime_error("index entry has out-of-range vertex start/count");
			}
			std::string name(&strings[0] + entry.name_begin, &strings[0] + entry.name_end);
			
			bool inserted = influences.insert(std::make_pair(name, std::vector < BoneInfluence >(infl_data.begin() + entry.vertex_begin, infl_data.begin() + entry.vertex_end))).second;
			if (!inserted) {
				std::cerr << "WARNING: mesh name '" + name + "' in filename '" + filename + "' collides with existing mesh." << std::endl;
			}
		}
		*/
	}

	if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in mesh file '" << filename << "'" << std::endl;
	}

	/* //DEBUG:
	std::cout << "File '" << filename << "' contained meshes";
	for (auto const &m : meshes) {
		if (&m.second == &meshes.rbegin()->second && meshes.size() > 1) std::cout << " and";
		std::cout << " '" << m.first << "'";
		if (&m.second != &meshes.rbegin()->second) std::cout << ",";
	}
	std::cout << std::endl;
	*/
}

/*
const std::vector < BoneInfluence > &BoneInfluenceBuffer::lookup(std::string const &name) const {
	auto f = influences.find(name);
	if (f == influences.end()) {
		throw std::runtime_error("Looking up mesh '" + name + "' that doesn't exist.");
	}
	return f->second;
}*/

GLuint RiggedMesh::make_vao_for_program(GLuint program_) {
	program = program_;

	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_vert);
	GLint location = glGetAttribLocation(program, "Position");
	if (location != -1) {
		glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Position));
		glEnableVertexAttribArray(location);
	}
	else {
		std::cerr << "WARNING: Failed to find 'Position' in program" << std::endl;
	}
	location = glGetAttribLocation(program, "Normal");
	if (location != -1) {
		glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Normal));
		glEnableVertexAttribArray(location);
	}
	else {
		std::cerr << "WARNING: Failed to find 'Normal' in program" << std::endl;
	}
	location = glGetAttribLocation(program, "Color");
	if (location != -1) {
		glVertexAttribPointer(location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Color));
		glEnableVertexAttribArray(location);
	}
	else {
		std::cerr << "WARNING: Failed to find 'Color' in program" << std::endl;
	}
	location = glGetAttribLocation(program, "TexCoord");
	if (location != -1) {
		glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, TexCoord));
		glEnableVertexAttribArray(location);
	}
	else {
		std::cerr << "WARNING: Failed to find 'TexCoord' in program" << std::endl;
	}

	glBindBuffer(GL_ARRAY_BUFFER, vbo_bone);
	location = glGetAttribLocation(program, "BoneIndices");
	if (location != -1) {
		glVertexAttribIPointer(location, 4, GL_UNSIGNED_INT, sizeof(BoneInfluence), (GLbyte *)0 + offsetof(BoneInfluence, BoneIndices));
		glEnableVertexAttribArray(location);
	}
	else {
		std::cerr << "WARNING: Failed to find 'BoneIndices' in program" << std::endl;
	}
	location = glGetAttribLocation(program, "BoneWeights");
	if (location != -1) {
		glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, sizeof(BoneInfluence), (GLbyte *)0 + offsetof(BoneInfluence, BoneWeights));
		glEnableVertexAttribArray(location);
	}
	else {
		std::cerr << "WARNING: Failed to find 'BoneWeights' in program" << std::endl;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return vao;
}

void RiggedMesh::update(float elapsed) {
	if (!anim_graph || program == 0) return;

	auto pose = skeleton.pose(anim_graph->sample());

	glUseProgram(program);
	glUniformMatrix4x3fv(glGetUniformLocation(program, "POSE"), (GLsizei) pose.size(), GL_FALSE, glm::value_ptr(pose[0]));
	glUseProgram(0);
}