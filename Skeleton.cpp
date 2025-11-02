#include "Skeleton.hpp"
#include "read_write_chunk.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <fstream>
#include <iostream>

const Skeleton::Bone &Skeleton::get_bone(std::string name) const {
    return bones[name_to_index.at(name)];
}

const Skeleton::Bone &Skeleton::get_bone(uint32_t index) const {
    return bones[index];
}

SkeletonBuffer::SkeletonBuffer(std::string const &filename) {
    std::ifstream file(filename, std::ios::binary);
    std::vector< Skeleton::Bone > data;

    if (filename.size() >= 5 && filename.substr(filename.size()-5) == ".skel") {
        read_chunk(file, "skel", &data);
    } else {
		throw std::runtime_error("Unknown file type '" + filename + "'");
	}

    struct Matrices {
        glm::mat4 skel_from_world;
        glm::mat4 world_from_skel;
    };
    static_assert(sizeof(Matrices) == 128 && "Matrices must be packed");
    std::vector< Matrices > mats;
    read_chunk(file, "mat0", &mats);

    std::vector< char > strings;
    read_chunk(file, "str0", &strings);
    
    {
        struct SkeletonIndexEntry {
            uint32_t name_begin, name_end;
            uint32_t bone_begin, bone_end;
        };
        static_assert(sizeof(SkeletonIndexEntry) == 16);

        struct BoneIndexEntry {
            uint32_t name_begin, name_end;
        };

        static_assert(sizeof(BoneIndexEntry) == 8);

        std::vector< SkeletonIndexEntry > skeleton_index;
        read_chunk(file, "idx0", &skeleton_index);

        std::vector< BoneIndexEntry > bone_index;
        read_chunk(file, "idx1", &bone_index);

        for (uint32_t i = 0; i < skeleton_index.size(); i++) {
            auto const &skel_entry = skeleton_index[i];
            if(!(skel_entry.name_begin <= skel_entry.name_end && skel_entry.name_end <= strings.size())) {
				throw std::runtime_error("index entry has out-of-range name begin/end");
            }
            if(!(skel_entry.bone_begin <= skel_entry.bone_end && skel_entry.bone_end <= strings.size())) {
				throw std::runtime_error("index entry has out-of-range skeleton begin/end");
            }

            std::string name(&strings[0] + skel_entry.name_begin, &strings[0] + skel_entry.name_end);
            Skeleton skeleton;
            skeleton.bones.assign( 
                data.begin() + skel_entry.bone_begin, 
                data.begin() + skel_entry.bone_end
            );

            skeleton.skel_from_world = mats[i].skel_from_world;
            skeleton.world_from_skel = mats[i].world_from_skel;
            
            // map bone name -> bone index
            skeleton.index_to_name.resize(skel_entry.bone_end - skel_entry.bone_begin);
            for (uint32_t j = 0; j < skel_entry.bone_end - skel_entry.bone_begin; ++j) {
                auto const &bone_entry = bone_index[skel_entry.bone_begin + j];
                std::string bone_name(&strings[0] + bone_entry.name_begin, &strings[0] + bone_entry.name_end);
                if (!skeleton.name_to_index.insert(std::make_pair(bone_name, j)).second) {
				    std::cerr << "WARNING: bone name '" + bone_name + "' in filename '" + filename + "' collides with existing bone." << std::endl;
                    continue;
                }
                skeleton.index_to_name[j] = bone_name;
            }

            if (!skeletons.insert(std::make_pair(name, skeleton)).second) {
				std::cerr << "WARNING: skeleton name '" + name + "' in filename '" + filename + "' collides with existing skeleton." << std::endl;
                continue;
            }
        }
    }

    if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in skeleton file '" << filename << "'" << std::endl;
    }
}

const Skeleton &SkeletonBuffer::lookup(std::string const &name) const {
    auto s = skeletons.find(name);
    if (s == skeletons.end()) {
		throw std::runtime_error("Looking up skeleton '" + name + "' that doesn't exist.");
    }
    return s->second;
}

void print_3(std::string title, glm::vec3 vec) {
    std::cout << "----" << title << std::endl;
    std::cout << vec.x << ", " << vec.y << ", " << vec.z << std::endl; 
}

void print_quat(std::string title, glm::quat vec) {
    std::cout << "----" << title << std::endl;
    std::cout << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << std::endl; 
}

void print_4x4(std::string title, glm::mat4 mat) {
    std::cout << "----" << title << std::endl;
    for (uint32_t j = 0; j < 4; ++j) {
        std::cout << mat[j][0]<< ", " << mat[j][1] << ", " << mat[j][2] << ", " << mat[j][3] << std::endl; 
    }
}
const Skeleton::Pose Skeleton::pose(const std::vector< BoneTransform > &pose_data) const {
    if (index_to_name.size() != pose_data.size()) {
        std::cerr << "WARNING: pose information must be specified for every bone in order" << std::endl;
        return {};
    }
    if (index_to_name.empty()) {
        std::cerr << "WARNING: cannot pose empty skeleton" << std::endl;
        return {};
    } 

    std::vector < glm::mat4 > skel_from_pose(bones.size());
    for (int32_t i = 0; i < index_to_name.size(); ++i) {
        const Bone &bone = bones[i];
        assert(bone.parent <= i);

        glm::mat4 T = glm::translate(glm::mat4(1.f), pose_data[i].position);
        glm::mat4 R = glm::mat4_cast(pose_data[i].rotation);
        glm::mat4 S = glm::scale(glm::mat4(1.f), pose_data[i].scale);

        glm::mat4 P = T*R*S;

        if (bone.parent == -1) {
            skel_from_pose[i] = bone.bind*P;
        }
        else {
            skel_from_pose[i] = skel_from_pose[bone.parent]*bones[bone.parent].inverse_bind*bone.bind*P;
        }
    }

    std::vector < glm::mat4x3 > res(bones.size());
    for (int32_t i = 0; i < index_to_name.size(); ++i) {
        res[i] = glm::mat4x3(skel_from_pose[i] * bones[i].inverse_bind);
    }
    return res;
}

/*
const std::vector< DynamicMeshBuffer::Vertex > Skeleton::skin(Skeleton::Pose &pose, const RiggedMesh &mesh) const {
    assert(mesh.groups.size() == mesh.vertices.size());
    std::vector< DynamicMeshBuffer::Vertex > res(mesh.vertices.size());

    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        glm::vec4 pos = glm::vec4(0.f); 
        glm::vec4 norm = glm::vec4(0.f);
        
        glm::vec4 v_skel = skel_from_world * mesh.world_from_mesh * glm::vec4(mesh.vertices[i].Position, 1.f);
        glm::vec4 n_skel = skel_from_world * mesh.world_from_mesh * glm::vec4(mesh.vertices[i].Normal, 0.f);
        for (size_t g = 0; g < mesh.groups[i].size(); ++g) {
            const Bone &bone = get_bone(mesh.groups[i][g].name);
            glm::mat4 P = pose[name_to_index.at(mesh.groups[i][g].name)];
            assert(P != glm::mat4(0.f));

            float w = mesh.groups[i][g].weight;
            pos += w * P * bone.inverse_bind * v_skel;
            norm += w * glm::transpose(glm::inverse(P * bone.inverse_bind)) * n_skel;
        }

        res[i].Position = glm::vec3(mesh.mesh_from_world * world_from_skel * pos);
        res[i].Normal = glm::normalize(norm);
        res[i].Color = mesh.vertices[i].Color;
        res[i].TexCoord = mesh.vertices[i].TexCoord;
    }

    return res;
}
*/
