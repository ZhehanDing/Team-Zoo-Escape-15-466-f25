#pragma once

#include "Mode.hpp"

#include "GL.hpp"
#include <functional>

#include "Scene.hpp"
#include "Overlay.hpp"

struct WinMode : Mode {
    WinMode();
    virtual ~WinMode();

    virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
    virtual void update(float elapsed) override;
    virtual void draw(glm::uvec2 const &drawable_size) override;

    Scene scene;
    Scene::Camera *camera = nullptr;
    Overlay overlay;
};