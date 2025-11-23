#pragma once

#include "Scene.hpp"
#include <glm/glm.hpp>

struct CollisionHits {
    bool out_of_bounds = false;
    bool success = false;

    bool gate = false;
    bool deer_fence = false;
    bool zoo_fence_near = false;
    bool zoo_fence_far = false;

    bool any() const {
        // printf("CollisionHits: out_of_bounds=%d, gate=%d, deer_fence=%d, zoo_fence_near=%d, zoo_fence_far=%d\n",
        //        out_of_bounds, gate, deer_fence, zoo_fence_near, zoo_fence_far);
        return out_of_bounds || gate || deer_fence || zoo_fence_near || zoo_fence_far;
    }

    bool escaped() const {
        return success;
    }
};

// Main function called from PlayMode.cpp
CollisionHits query_world_collisions(
    const glm::vec3 &new_pos,
    Scene::Transform *gate,
    Scene::Transform *deer_fence_collider,
    Scene::Transform *zoo_fence_near_collider,
    Scene::Transform *zoo_fence_far_collider
);

// Simple AABB test
bool check_collision(
    const glm::vec3 &posA, const glm::vec3 &halfA,
    const glm::vec3 &posB, const glm::vec3 &halfB
);
