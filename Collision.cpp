#include "Collision.hpp"
#include <cmath>

// --- AABB intersection ---
bool check_collision(
    const glm::vec3 &posA, const glm::vec3 &halfA,
    const glm::vec3 &posB, const glm::vec3 &halfB
) {
    return (std::abs(posA.x - posB.x) <= (halfA.x + halfB.x)) &&
           (std::abs(posA.y - posB.y) <= (halfA.y + halfB.y)) &&
           (std::abs(posA.z - posB.z) <= (halfA.z + halfB.z));
}


// --- Shared half-extents, created once ---
namespace {
    const glm::vec3 PLAYER_HALF            = glm::vec3(1.0f, 1.0f, 1.0f);
    const glm::vec3 GATE_HALF              = glm::vec3(2.0f, 9.0f, 3.0f);
    const glm::vec3 DEER_FENCE_HALF        = glm::vec3(35.0f, 35.0f, 3.0f);
    const glm::vec3 ZOO_FENCE_NEAR_HALF    = glm::vec3(60.0f, 60.0f, 3.0f);
}


// --- Public function used by PlayMode.cpp ---
CollisionHits query_world_collisions(
    const glm::vec3 &new_pos,
    Scene::Transform *gate,
    Scene::Transform *deer_fence_collider,
    Scene::Transform *zoo_fence_near_collider
) {
    CollisionHits hits;

    if (gate) {
        hits.gate = check_collision(new_pos, PLAYER_HALF, gate->position, GATE_HALF);
    }
    if (deer_fence_collider) {
        hits.deer_fence = check_collision(new_pos, PLAYER_HALF, deer_fence_collider->position, DEER_FENCE_HALF);
    }
    if (zoo_fence_near_collider) {
        hits.zoo_fence_near = check_collision(new_pos, PLAYER_HALF, zoo_fence_near_collider->position, ZOO_FENCE_NEAR_HALF);
    }

    return hits;
}
