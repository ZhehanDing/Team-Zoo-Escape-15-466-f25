#pragma once

#include "GL.hpp"
#include <glm/glm.hpp>
#include <map>
#include <limits>
#include <string>
#include <vector>

#include "Mesh.hpp"
#include "Skeleton.hpp"
#include "Animation.hpp"

struct Vertex {
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::u8vec4 Color;
	glm::vec2 TexCoord;
};
static_assert(sizeof(Vertex) == 4*3+4*3+4+4*2);

struct BoneInfluence {
	// per provided 15466 BoneAnimation, vertex only influenced by at most 4 bones
	// -- easier to push fixed than varying amt of bone influences to gpu
	glm::uvec4 BoneIndices;
	glm::vec4 BoneWeights; 
};
static_assert(sizeof(BoneInfluence) == (4+4)*4);

// start and count of group influences are exactly those given by a mesh.
// in other words, one can optionally use a mesh's vertex group influences, 
// but group influences should not be used without the corresponding mesh
struct BoneInfluenceBuffer {
	//construct from a file:
	// note: will throw if file fails to read.
	BoneInfluenceBuffer(std::string const &filename);

	//look up a particular mesh by name:
	// note: will throw if mesh not found.
	const std::vector < BoneInfluence > &lookup(std::string const &name) const;

	MeshBuffer::Attrib BoneIndices;
	MeshBuffer::Attrib BoneWeights;

	// buffer storing bone index/weight information
	GLuint buffer = 0;
	GLuint total = 0;

	// -- internals --
	//used by the lookup() function:
	//std::map< std::string, std::vector< BoneInfluence > > influences;
};

struct RiggedMesh {
	const Mesh &mesh;
	const Skeleton &skeleton;
	AnimationGraph< Skeleton::BoneTransform > *anim_graph = nullptr;

	// source is MeshBuffer::buffer where the mesh data exists for purpose of vao creation
	RiggedMesh(
		GLuint vbo_vert, 
		GLuint vbo_bone, 
		const Mesh &mesh, 
		const Skeleton &skeleton) 
		: vbo_vert(vbo_vert), 
		  vbo_bone(vbo_bone), 
		  mesh(mesh), 
		  skeleton(skeleton) 
		{ assert(vbo_vert > 0 && vbo_bone > 0); };
	RiggedMesh(
		GLuint vbo_vert, 
		GLuint vbo_bone, 
		const Mesh &mesh,
		const Skeleton &skeleton, 
		AnimationGraph< Skeleton::BoneTransform > *anim_graph) 
		: vbo_vert(vbo_vert), 
		  vbo_bone(vbo_bone), 
		  mesh(mesh), 
		  skeleton(skeleton), 
		  anim_graph(anim_graph) 
		{ assert(vbo_vert > 0 && vbo_bone > 0); };

	// binds the mesh to the skeleton via bone weight information
	//void bind(std::vector < BoneInfluence > infls);
	void update(float elapsed);

	//glm::mat4 mesh_from_world;
	//glm::mat4 world_from_mesh;

	GLuint program = 0;

	GLuint vbo_vert = 0;
	GLuint vbo_bone = 0;
	GLuint make_vao_for_program(GLuint program);
};
