#pragma once

#include "glm/glm.hpp"
#include "Scene.hpp"
#include "DrawLines.hpp"

struct Collider {
    glm::vec3 size = glm::vec3(1.f);
    glm::vec3 offset = glm::vec3(0.f);

    glm::mat4x3 make_world_from_local();

    Collider(Scene::Transform *anchor) : anchor(anchor) { assert(anchor); };
    Collider(const glm::vec3 &position, const glm::vec3 &scale) : anchor(new Scene::Transform()) { 
        assert(anchor); 
        anchor->position = position;
        anchor->scale = scale;
    };

    void set_bounds(glm::vec3 min, glm::vec3 max);

    bool intersect(Collider other);
    // given a start position and end, if movement from "start" to "end" were to cross
    // the collider, clamp movement respectively, and account for collider width
    // updates the end position
    bool clip_movement(Collider other, glm::highp_vec3 &dir, float &dist, uint8_t granularity=3);

    private:
        Scene::Transform *anchor;
};