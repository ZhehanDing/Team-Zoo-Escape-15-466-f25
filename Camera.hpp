#pragma once

#include "Scene.hpp"
#include "Collision.hpp"
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

struct Camera {
    Scene::Camera *main = nullptr;
    Scene::Transform *anchor = nullptr;

    glm::vec3 offset = glm::vec3(0.f);
    glm::vec2 pitch_range = glm::vec2(-(float)M_PI, 0.f);
    glm::vec2 yaw_range = glm::vec2(-(float) M_PI, (float)M_PI);
    float sensitivity = 2.f;
    float pitch, roll, yaw, distance;
    bool rotate_anchor = false;

    void set_initial_look_radians(float pitch_, float roll_, float yaw_) {
        pitch = pitch_;
        roll = roll_;
        yaw = yaw_;
    };
    void set_initial_look_degrees(float pitch_, float roll_, float yaw_) { set_initial_look_radians(glm::radians(pitch_), glm::radians(roll_), glm::radians(yaw_)); };

    void set_sensitivity(float sensitivity_) {
        sensitivity = sensitivity_;
    };
    
    void set_orbit_offset_from_anchor(glm::vec3 offset_) {
        offset = offset_;
    };
    
    void set_max_distance_from_camera_center(float distance_) {
        distance = distance_;
    }
    
    void set_pitch_range(float min, float max) {
        pitch_range.x = min;
        pitch_range.y = max;
    };
    void set_pitch_range(glm::vec2 range) { set_pitch_range(range.x, range.y); }

    void update_camera(float delta_mouse_x, float delta_mouse_y, std::vector< BBox > const *objects);
    void update_camera(glm::vec2 delta_mouse, std::vector< BBox > const *objects) { update_camera(delta_mouse.x, delta_mouse.y, objects); };
    
    Camera(Scene::Camera *camera, Scene::Transform *anchor) : main(camera), anchor(anchor) { 
        assert(main); 
        assert(anchor); 
        main->transform->parent = anchor;
    };
};