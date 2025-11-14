#pragma once

#include <random>
#include <glm/glm.hpp>

#include "Scene.hpp"

/*
// looking into type-erasure, may change such that any user-defined sample function can be used

template < typename T >
struct Sampler {
    static virtual T sample(std::mt19937 &rng);
};

// uniform sphere distribution https://mathworld.wolfram.com/SpherePointPicking.html
struct Sphere : public Sampler< glm::vec3 > {
    virtual glm::vec3 sample(std::mt19937 &rng) override { 
        auto dist = std::uniform_real_distribution< float > (-1.f, 1.f);
        
        float x1, x2, x1_sqr, x2_sqr;
        do {
            x1 = dist(rng);
            x2 = dist(rng);

            x1_sqr = x1 * x1;
            x2_sqr = x2 * x2;
        }
        while (x1_sqr + x2_sqr >= 1.f);
        
        float S = std::sqrt(1 - x1_sqr - x2_sqr);
        glm::vec3 dir = glm::normalize(glm::vec3(2.f * x1 * S, 2.f * x2 * S, 1.f - 2.f * (x1_sqr + x2_sqr)));

        return transform->position + dir; 
    }

    Sphere(Scene::Transform *transform, float radius) : transform(transform), radius(radius) { assert(transform); };

    private:
        Scene::Transform *transform;
        float radius;
};
*/

// uniform sphere distribution https://mathworld.wolfram.com/SpherePointPicking.html
struct Sphere {
    static glm::vec3 sample(std::mt19937 &rng) {
        auto dist = std::uniform_real_distribution< float > (-1.f, 1.f);
        
        float x1, x2, x1_sqr, x2_sqr;
        do {
            x1 = dist(rng);
            x2 = dist(rng);

            x1_sqr = x1 * x1;
            x2_sqr = x2 * x2;
        }
        while (x1_sqr + x2_sqr >= 1.f);

        float S = std::sqrt(1 - x1_sqr - x2_sqr);
        glm::vec3 p(2.f * x1 * S, 2.f * x2 * S, 1.f - 2.f * (x1_sqr + x2_sqr));

        return p; 
    };
};