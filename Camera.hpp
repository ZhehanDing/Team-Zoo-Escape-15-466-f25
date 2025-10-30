#pragma once

#include "Scene.hpp"
#include <iostream>

struct Camera {
    Scene::Camera *main = nullptr;
    Scene::Transform *anchor = nullptr;

    glm::vec3 offset = glm::vec3(0.f);
    glm::vec2 pitch_range = glm::vec2(-(float)M_PI, 0.f);
    glm::vec2 yaw_range = glm::vec2(-(float) M_PI, (float)M_PI);
    float sensitivity = 2.f;
    float pitch, roll, yaw, distance;

    void set_initial_look_radians(float pitch_, float roll_, float yaw_) {
        pitch = pitch_;
        roll = roll_;
        yaw = yaw_;
        update_camera(0.f, 0.f);
    };
    void set_initial_look_degrees(float pitch_, float roll_, float yaw_) { set_initial_look_radians(glm::radians(pitch_), glm::radians(roll_), glm::radians(yaw_)); };

    void set_sensitivity(float sensitivity_) {
        sensitivity = sensitivity_;
    };
    
    void set_orbit_offset_from_anchor(glm::vec3 offset_) {
        offset = offset_;
        update_camera(0.f, 0.f);
    };
    
    void set_max_distance_from_camera_center(float distance_) {
        distance = distance_;
        update_camera(0.f, 0.f);
    }
    
    void set_pitch_range(float min, float max) {
        pitch_range.x = min;
        pitch_range.y = max;
        update_camera(0.f, 0.f);
    };
    void set_pitch_range(glm::vec2 range) { set_pitch_range(range.x, range.y); }

    void update_camera(float delta_mouse_x, float delta_mouse_y) {
        { // update camera rotation
            yaw -= delta_mouse_x * main->fovy * sensitivity;
            pitch += delta_mouse_y * main->fovy * sensitivity;
            
            if (yaw > yaw_range.y) yaw -= (yaw_range.y - yaw_range.x);
            else if (yaw < yaw_range.x) yaw += (yaw_range.y - yaw_range.x);

            if (pitch > pitch_range.y) pitch = pitch_range.y;
            else if (pitch < pitch_range.x) pitch = pitch_range.x;

            if (!main->transform->parent)
                main->transform->rotation = glm::quat( glm::vec3 (pitch, roll, yaw) );
            else {
                main->transform->rotation = glm::quat(glm::vec3 (pitch, roll, 0.f));
                glm::vec3 rot = glm::eulerAngles(main->transform->parent->rotation);
                anchor->rotation = glm::quat( glm::vec3 (rot.x, rot.y, yaw));
            }

            glm::mat4x3 frame = main->transform->make_parent_from_local();
            glm::vec3 frame_at = frame[2];
            main->transform->position = offset + frame_at * distance;
        }
    };
    void update_camera(glm::vec2 delta_mouse) { update_camera(delta_mouse.x, delta_mouse.y); };
    
    Camera(Scene::Camera *camera, Scene::Transform *anchor) : main(camera), anchor(anchor) { 
        assert(main); 
        assert(anchor); 
        main->transform->parent = anchor;
    };
};