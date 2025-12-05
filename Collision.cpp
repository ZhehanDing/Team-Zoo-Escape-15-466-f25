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
    const glm::vec3 DEER_FENCE_HALF        = glm::vec3(34.0f, 39.0f, 3.0f);
    const glm::vec3 ZOO_FENCE_NEAR_HALF    = glm::vec3(58.0f, 60.0f, 3.0f);
    const glm::vec3 ZOO_FENCE_FAR_HALF    = glm::vec3(58.0f, 49.0f, 3.0f);
    const glm::vec3 SMALL_HOUSE_HALF    = glm::vec3(5.5f, 7.2f, 3.0f);
    const glm::vec3 SMALL_HOUSE_2_HALF    = glm::vec3(2.0f, 3.83f, 3.0f);
    const glm::vec3 CAR_HALF    = glm::vec3(1.4f, 4.0f, 1.0f);
}

namespace { // bounds for the playable area
    constexpr float MIN_Y = -80.0f;
    constexpr float MAX_Y = 90.0f;
    constexpr float MAX_X = 70.0f;
    constexpr float SUCCESS_X = -100.0f;
}

// --- Public function used by PlayMode.cpp ---
CollisionHits query_world_collisions(
    const glm::vec3 &new_pos,
    Scene::Transform *gate,
    Scene::Transform *deer_fence_collider,
    Scene::Transform *zoo_fence_near_collider,
    Scene::Transform *zoo_fence_far_collider,
    Scene::Transform *small_house_collider,
    Scene::Transform *small_house_2_collider,
    Scene::Transform *car_collider,
    Scene::Transform *car_collider_2
) {
    CollisionHits hits;

    if (new_pos.y < MIN_Y || new_pos.y > MAX_Y || new_pos.x > MAX_X) {
        hits.out_of_bounds = true;
        return hits;
    }

    if (new_pos.x < SUCCESS_X) {
        hits.success = true;
        return hits;
    }

    if (gate) {
        hits.gate = check_collision(new_pos, PLAYER_HALF, gate->position, GATE_HALF);
    }
    if (deer_fence_collider) {
        hits.deer_fence = check_collision(new_pos, PLAYER_HALF, deer_fence_collider->position, DEER_FENCE_HALF);
    }
    if (zoo_fence_near_collider) {
        hits.zoo_fence_near = check_collision(new_pos, PLAYER_HALF, zoo_fence_near_collider->position, ZOO_FENCE_NEAR_HALF);
    }
    if (zoo_fence_far_collider) {
        hits.zoo_fence_far = check_collision(new_pos, PLAYER_HALF, zoo_fence_far_collider->position, ZOO_FENCE_FAR_HALF);
    }
    if (small_house_collider) {
        hits.small_house = check_collision(new_pos, PLAYER_HALF, small_house_collider->position, SMALL_HOUSE_HALF);
    }
    if (small_house_2_collider) {
        hits.small_house_2 = check_collision(new_pos, PLAYER_HALF, small_house_2_collider->position, SMALL_HOUSE_2_HALF);
    }
    if (car_collider) {
        hits.car = check_collision(new_pos, PLAYER_HALF, car_collider->position, CAR_HALF);
    }
    if (car_collider_2) {
        hits.car_2 = check_collision(new_pos, PLAYER_HALF, car_collider_2->position, CAR_HALF);
    }

    return hits;
}
