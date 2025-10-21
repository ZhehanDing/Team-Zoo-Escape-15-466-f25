#pragma once

#include "glm/glm.hpp"
#include "Scene.hpp"
#include "DrawLines.hpp"

struct Collider {
    glm::vec3 size = glm::vec3(1.f);
    glm::vec3 offset = glm::vec3(0.f);

    glm::mat4x3 get_transformation_matrix();

    Collider(Scene::Transform *obj_transform) : obj_transform(obj_transform) { assert(obj_transform); };
    Collider(const glm::vec3 &position, const glm::vec3 &scale) : obj_transform(new Scene::Transform()) { 
        assert(obj_transform); 
        obj_transform->position = position;
        obj_transform->scale = scale;
    };

    bool intersect(Collider other);
    // given a start position and end, if movement from "start" to "end" were to cross
    // the collider, clamp movement respectively, and account for collider width
    // updates the end position
    bool clip_movement(Collider other, glm::highp_vec3 &dir, float &dist, uint8_t granularity=3);

    private:
        Scene::Transform *obj_transform;
};