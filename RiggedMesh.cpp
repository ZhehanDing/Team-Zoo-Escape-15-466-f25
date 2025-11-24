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
	if (!anim_graph || program == 0) {
		printf("WARNING: RiggedMesh::update() called with !anim_graph: %p or program == 0: %u\n", (void *) anim_graph, program);
		return;
	}

	auto pose = skeleton.pose(anim_graph->sample());

	constexpr size_t MAX_BONES = 256;

	std::vector< glm::mat4 > pose4;
	pose4.reserve(pose.size());
	for (size_t i = 0; i < pose.size(); ++i) {
		glm::mat4x3 m3 = pose[i];
		glm::mat4 m4(1.0f);
		for (int c = 0; c < 4; ++c) {
			glm::vec3 col = m3[c];
			m4[c] = glm::vec4(col, (c == 3) ? 1.0f : 0.0f);
		}
		pose4.push_back(m4);
	}

	if (pose_ubo == 0) {
		glGenBuffers(1, &pose_ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, pose_ubo);
		glBufferData(GL_UNIFORM_BUFFER, MAX_BONES * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glBindBuffer(GL_UNIFORM_BUFFER, pose_ubo);
	if (!pose4.empty()) {
		glBufferSubData(GL_UNIFORM_BUFFER, 0, pose4.size() * sizeof(glm::mat4), glm::value_ptr(pose4[0]));
	}
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void RiggedMesh::bind_pose_ubo() const {
	if (pose_ubo == 0) return;
	constexpr GLuint POSE_BINDING = 3;
	glBindBufferBase(GL_UNIFORM_BUFFER, POSE_BINDING, pose_ubo);
}