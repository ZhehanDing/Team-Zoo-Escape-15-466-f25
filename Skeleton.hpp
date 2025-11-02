#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include <string>
#include <vector>
#include <span>

// Structured and inspired by Mesh.hpp/cpp
struct Skeleton {
    struct BoneTransform {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
    };
    static_assert(sizeof(BoneTransform) == 4*4+4*3+4*3);

    struct Bone {
        int32_t parent;
        glm::mat4 inverse_bind;
        glm::mat4 bind;
    };
    static_assert(sizeof(Bone) == 4+4*4*4+4*4*4);
    
    std::vector< Bone > bones;

    glm::mat4 skel_from_world;
    glm::mat4 world_from_skel;
    
    const Bone &get_bone(std::string name) const;
    const Bone &get_bone(uint32_t index) const;


    typedef std::vector< glm::mat4x3 > Pose;

    const Pose pose(const std::vector< BoneTransform > &pose_data) const;
    //const std::vector< Vertex > skin(Pose &pose, const RiggedMesh &mesh) const;

    // -- internals --
    std::vector< std::string > index_to_name;
    std::map< std::string, uint32_t > name_to_index;
};

struct SkeletonBuffer {
    SkeletonBuffer(std::string const &filename);

    const Skeleton &lookup(std::string const &name) const;

    // -- internals --
    // store skeletons at rest position
    std::map< std::string, Skeleton > skeletons;
};
