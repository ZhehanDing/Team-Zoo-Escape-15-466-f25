#include "Camera.hpp"

void Camera::update_camera(float delta_mouse_x, float delta_mouse_y, std::vector< BBox > const *objects) {
    yaw -= delta_mouse_x * main->fovy * sensitivity;
    pitch += delta_mouse_y * main->fovy * sensitivity;
    
    if (yaw > yaw_range.y) yaw -= (yaw_range.y - yaw_range.x);
    else if (yaw < yaw_range.x) yaw += (yaw_range.y - yaw_range.x);

    if (pitch > pitch_range.y) pitch = pitch_range.y;
    else if (pitch < pitch_range.x) pitch = pitch_range.x;

    if (!main->transform->parent || !rotate_anchor) {
        glm::mat4 to_world = glm::mat4(main->transform->parent->make_world_from_local());
        glm::vec3 scale, translation, skew;
        glm::quat orientation;
        glm::vec4 perspective;
        glm::decompose(to_world, scale, orientation, translation, skew, perspective);
        main->transform->rotation = glm::inverse(
            glm::quat_cast(to_world * glm::inverse(glm::scale(scale)))) * 
            glm::quat( glm::vec3 (pitch, roll, yaw) );
    }
    else {
        main->transform->rotation = glm::quat(glm::vec3 (pitch, roll, 0.f));
        glm::vec3 rot = glm::eulerAngles(main->transform->parent->rotation);
        anchor->rotation = glm::quat( glm::vec3 (rot.x, rot.y, yaw));
    }

    glm::mat4x3 frame = main->transform->make_parent_from_local();
    glm::vec3 frame_at = frame[2];


    // crop distance, similar to 15-(3/4)62

    float closest = distance;
    glm::vec3 world_dir = glm::normalize(main->transform->make_world_from_local()[2]);
    glm::vec3 world_pos = main->transform->parent->make_world_from_local() * glm::vec4(offset, 1.f);
    if (objects) {
        for (auto obj : *objects) {
            auto MI = obj.transform->make_local_from_world();
            glm::vec3 bounds[2] = { obj.mesh.min, obj.mesh.max };

            glm::vec3 local_dir = MI * glm::vec4(world_dir, 0.f);
            glm::vec3 local_inv_dir = 1.f / local_dir;
            glm::vec3 local_pos = MI * glm::vec4(world_pos, 1.f);

            int sign[3];
            sign[0] = local_dir.x < 0;
            sign[1] = local_dir.y < 0;
            sign[2] = local_dir.z < 0;

            float tmin, tmax, tymin, tymax, tzmin, tzmax;
            tmin = (bounds[sign[0]].x - local_pos.x) * local_inv_dir.x;
            tmax = (bounds[1 - sign[0]].x - local_pos.x) * local_inv_dir.x;
            tymin = (bounds[sign[1]].y - local_pos.y) * local_inv_dir.y;
            tymax = (bounds[1 - sign[1]].y - local_pos.y) * local_inv_dir.y;

            if (tmin > tymax || tymin > tmax) {
                continue;
            }

            if (tymin > tmin) {
                tmin = tymin;
            }
            if (tymax < tmax) {
                tmax = tymax;
            }

            tzmin = (bounds[sign[2]].z - local_pos.z) * local_inv_dir.z;
            tzmax = (bounds[1 - sign[2]].z - local_pos.z) * local_inv_dir.z;

            if (tmin > tzmax || tzmin > tmax) {
                continue;
            }

            if (tzmin > tmin) {
                tmin = tzmin;
            }
            if (tzmax < tmax) {
                tmax = tzmax;
            }

            if (tmax < 0.f || tmin > closest) continue;
            if (tmin < 0.f) tmin = tmax;
            if (tmin >= 0.f && tmin < closest) {
                closest = std::max(tmin - .25f, 0.f);
            }
        }
    }

    main->transform->position = offset + frame_at * closest;

    forward_xy = glm::vec3 (
        std::sinf(yaw), 
        -std::cos(yaw), 
        0.f);
    right_xy = glm::vec3 (
        forward_xy.y, 
        -forward_xy.x, 
        0.f);
}