#pragma once

/*
 * In this code, "Mesh" is a range of vertices that should be sent through
 *  the OpenGL pipeline together.
 * A "MeshBuffer" holds a collection of such meshes (loaded from a file) in
 *  a single OpenGL array buffer. Individual meshes can be looked up by name
 *  using the MeshBuffer::lookup() function.
 *
 */

#include "GL.hpp"
#include <glm/glm.hpp>
#include <map>
#include <limits>
#include <string>
#include <vector>

#include "DynamicMeshBuffer.hpp"

struct VertexGroup {
	std::string name;
	float weight;
};

struct VertexGroupIndex {
	uint32_t begin;
	uint32_t end;
};
static_assert(sizeof(VertexGroupIndex) == 8);

struct RiggedMesh {
	//Meshes are vertex ranges (and primitive types) in their MeshBuffer:

	std::vector< DynamicMeshBuffer::Vertex > vertices;
	std::vector< std::vector< VertexGroup > > groups;

	glm::mat4 mesh_from_world;
	glm::mat4 world_from_mesh;

	//Bounding box.
	//useful for debug visualization and (perhaps, eventually) collision detection:
	glm::vec3 min = glm::vec3( std::numeric_limits< float >::infinity());
	glm::vec3 max = glm::vec3(-std::numeric_limits< float >::infinity());
};

struct RiggedMeshBuffer {
	//construct from a file:
	// note: will throw if file fails to read.
	RiggedMeshBuffer(std::string const &filename);

	//look up a particular mesh by name:
	// note: will throw if mesh not found.
	const RiggedMesh &lookup(std::string const &name) const;

	//-- internals ---

	std::vector< DynamicMeshBuffer::Vertex > vertex_data;
	std::vector< VertexGroup > group_data;
	std::vector< VertexGroupIndex > group_index;

	//used by the lookup() function:
	std::map< std::string, RiggedMesh > meshes;
};
