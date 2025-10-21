#include "RiggedMesh.hpp"
#include "read_write_chunk.hpp"

#include <glm/glm.hpp>

#include <stdexcept>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <cstddef>

RiggedMeshBuffer::RiggedMeshBuffer(std::string const &filename) {
	std::ifstream file(filename, std::ios::binary);

	//read + upload data chunk:
	if (filename.size() >= 5 && filename.substr(filename.size()-5) == ".pnct") {
		read_chunk(file, "pnct", &vertex_data);
	} else {
		throw std::runtime_error("Unknown file type '" + filename + "'");
	}

	group_data.resize(vertex_data.size());

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

		std::vector< char > group_names;
		read_chunk(file, "grp0", &group_names);

		struct VertexGroupEntry {
			uint32_t name_begin;
			uint32_t name_end;
			float weight;
		};
		static_assert(sizeof(VertexGroupEntry) == 12, "Vertex group data should be packed");

		std::vector< VertexGroupEntry > vg_data;
		read_chunk(file, "vgd0", &vg_data);

		read_chunk(file, "vgi0", &group_index);

		assert(group_index.size() == vertex_data.size());

		struct Matrices {
			glm::mat4 mesh_from_world;
			glm::mat4 world_from_mesh;
		};
		static_assert(sizeof(Matrices) == 128, "Matrices data should be packed");
		
		std::vector< Matrices > mats;
		read_chunk(file, "mat0", &mats);

		group_data.resize(vg_data.size());
		for (size_t i = 0; i < index.size(); ++i) {
			auto const &entry = index[i];
			if (!(entry.name_begin <= entry.name_end && entry.name_end <= strings.size())) {
				throw std::runtime_error("index entry has out-of-range name begin/end");
			}
			if (!(entry.vertex_begin <= entry.vertex_end && entry.vertex_end <= vertex_data.size())) {
				throw std::runtime_error("index entry has out-of-range vertex start/count");
			}
			std::string name(&strings[0] + entry.name_begin, &strings[0] + entry.name_end);
			
			RiggedMesh mesh;
			mesh.vertices = std::vector< DynamicMeshBuffer::Vertex >(vertex_data.begin() + entry.vertex_begin, vertex_data.begin() + entry.vertex_end);
			mesh.groups.resize(entry.vertex_end - entry.vertex_begin);
			for (uint32_t v = entry.vertex_begin; v < entry.vertex_end; ++v) {
				mesh.min = glm::min(mesh.min, vertex_data[v].Position);
				mesh.max = glm::max(mesh.max, vertex_data[v].Position);

				VertexGroupIndex vgi_entry = group_index[v];
				if (!(vgi_entry.begin <= vgi_entry.end && vgi_entry.end <= vg_data.size())) {
					throw std::runtime_error("vg index entry has out-of-range name begin/end");
				}

				for (uint32_t j = vgi_entry.begin; j < vgi_entry.end; ++j) {
					VertexGroupEntry vg_entry = vg_data[j];
					if (!(vg_entry.name_begin <= vg_entry.name_end && vg_entry.name_end <= group_names.size())) {
						throw std::runtime_error("vg index entry has out-of-range name begin/end");
					}

					std::string g_name(&group_names[0] + vg_entry.name_begin, &group_names[0] + vg_entry.name_end);
					group_data[j].name = g_name;
					group_data[j].weight = vg_entry.weight;
				}

				mesh.groups[v - entry.vertex_begin] = (std::vector< VertexGroup >(group_data.begin() + vgi_entry.begin, group_data.begin() + vgi_entry.end));
			}


			mesh.mesh_from_world = mats[i].mesh_from_world;
			mesh.world_from_mesh= mats[i].world_from_mesh;

			bool inserted = meshes.insert(std::make_pair(name, mesh)).second;
			if (!inserted) {
				std::cerr << "WARNING: mesh name '" + name + "' in filename '" + filename + "' collides with existing mesh." << std::endl;
			}
		}
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

const RiggedMesh &RiggedMeshBuffer::lookup(std::string const &name) const {
	auto f = meshes.find(name);
	if (f == meshes.end()) {
		throw std::runtime_error("Looking up mesh '" + name + "' that doesn't exist.");
	}
	return f->second;
}
