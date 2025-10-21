#include "Collision.hpp"
#include "DrawLines.hpp"

#include <iostream>

glm::mat4x3 Collider::get_transformation_matrix() {
    return obj_transform->make_world_from_local() *
        glm::mat4(
            size.x, 0.f, 0.f, offset.x,
            0.f, size.y, 0.f, offset.y,
            0.f, 0.f, size.z, offset.z,
            0.f, 0.f, 0.f, 1.f
        );
}

// https://developer.mozilla.org/en-US/docs/Games/Techniques/3D_collision_detection
// need to update with local from world transformation into one of the two local spaces
bool Collider::intersect(Collider other) {
    glm::highp_vec3 min = obj_transform->position + offset - size;
    glm::highp_vec3 max = obj_transform->position + offset + size;

    glm::highp_vec3 other_min = other.obj_transform->position + other.offset - other.size;
    glm::highp_vec3 other_max = other.obj_transform->position + other.offset + other.size;

    return min.x <= other_max.x &&
        max.x >= other_min.x &&
        min.y <= other_max.y &&
        max.y >= other_min.y &&
        min.z <= other_max.z &&
        max.z >= other_min.z;
}

// width of box collider is accounted for it you swap the objects in the call, and swap the start and end.
//  i.e. for BOXES A and B, if A is moving towards static B,
//         old_target = target
//         A.clip_movement(B, A.position, &target) -> clips target
//         
// (only problem is that this happens at the center only---if it causes problems for this game, perform this
//  over a small grid based on size of collider)
bool Collider::clip_movement(Collider other, glm::highp_vec3 &dir, float &dist, uint8_t granularity) {
    if (dist == 0.f) return false;
    
    // convert world direction and world position to this object space
    glm::highp_vec3 dir_obj_space = other.obj_transform->make_local_from_world() * glm::highp_vec4(dir, 0.f);

    // my (modified) ray-bbox intersection code from 15-362 computer graphics
    // based on scratchpixel
    glm::highp_vec3 other_min = glm::highp_vec4(other.obj_transform->make_local_from_world() * glm::highp_vec4(other.obj_transform->position, 1.f) + other.offset - other.size, 1.f);
    glm::highp_vec3 other_max = glm::highp_vec4(other.obj_transform->make_local_from_world() * glm::highp_vec4(other.obj_transform->position, 1.f) + other.offset + other.size, 1.f);

    glm::highp_vec3 min = glm::highp_vec4(other.obj_transform->make_local_from_world() * glm::highp_vec4(obj_transform->position + offset - size, 1.f), 1.f);
    glm::highp_vec3 max = glm::highp_vec4(other.obj_transform->make_local_from_world() * glm::highp_vec4(obj_transform->position + offset + size, 1.f), 1.f);

    glm::highp_vec3 other_bounds[2] = { other_min, other_max };
    glm::highp_vec3 bounds[2] = { min, max };

    glm::highp_vec3 invdir = 1.f / dir_obj_space;

    glm::highp_vec3 time(1.f);
    glm::highp_vec3 new_dir(dir);

    glm::highp_vec3 vertices[4];
    bool clipped = false;
    // for each dimension
    for (glm::length_t i = 0; i < 3; i++) {
        int sign = dir_obj_space[i] < 0;

        // obtain the vertices on the appropriate face
        //glm::vec3 vertices[4];
        uint8_t b = 0; // tmp to check if smallest dim has been filled
        for (glm::length_t j = 0; j < 2; j++) {
            for (glm::length_t d = 0; d < 3; d++) {
                if (d == i) {
                    // choose the face closest to the collider
                    vertices[2 * j][d] = sign == 0 ? max[d] : min[d];
                    vertices[2*j+1][d] = sign == 0 ? max[d] : min[d];
                }
                else if (b == 0) {
                    // choose the same min or max
                    vertices[2 * j][d] = bounds[j & 0b1][d];
                    vertices[2*j+1][d] = bounds[j & 0b1][d];
                    b += 1;
                }
                else { //b == 1
                    // choose min and max
                    vertices[2 * j][d] = bounds[0][d];
                    vertices[2*j+1][d] = bounds[1][d];
                }
            }
        }

        float tmin, tmax;
        for (size_t j = 0; j < 4; j++) {
            bool inside = true;
            for (glm::length_t d = 0; d < 3; d++) {
                if (d == i) continue;

                inside = inside &&
                    vertices[j][d] >= other_min[d] &&
                    vertices[j][d] <= other_max[d];
            }

            if (!inside) {
                continue;
            }

            if (dir_obj_space[i] != 0.f) {
                // if the direction of movement does hit the face
                tmin = (other_bounds[sign][i] - vertices[j][i]) * invdir[i];
                tmax = (other_bounds[1 - sign][i] - vertices[j][i]) * invdir[i];
            }
            else {
                tmin = -std::numeric_limits<float>::infinity();
                tmax = std::numeric_limits<float>::infinity();
            }   

            // checking dist bounds
            if (tmax < 0.f) {
                continue;
            }
            clipped |= true;
            time[i] = std::min(std::max(tmin, 0.f), time[i]);
            new_dir[i] = dir[i] * time[i];
        }
    }

    dir = new_dir;
    return clipped;
}
