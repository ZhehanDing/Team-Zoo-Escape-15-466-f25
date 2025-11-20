#pragma once

#include "Scene.hpp"
#include <glm/glm.hpp>

struct CollisionHits {
    bool gate = false;
    bool deer_fence = false;
    bool zoo_fence_near = false;

    bool any() const {
        return gate || deer_fence || zoo_fence_near;
    }
};

// Main function called from PlayMode.cpp
CollisionHits query_world_collisions(
    const glm::vec3 &new_pos,
    Scene::Transform *gate,
    Scene::Transform *deer_fence_collider,
    Scene::Transform *zoo_fence_near_collider
);

// Simple AABB test
bool check_collision(
    const glm::vec3 &posA, const glm::vec3 &halfA,
    const glm::vec3 &posB, const glm::vec3 &halfB
);
