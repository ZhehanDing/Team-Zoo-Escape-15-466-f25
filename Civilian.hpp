#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <memory>
#include "Scene.hpp"
#include "Skeleton.hpp"
#include "RiggedMesh.hpp"
#include "Animation.hpp"

struct Civilian {
	Scene::Transform *transform = nullptr;
	std::unique_ptr< Skeleton > skel;
	std::unique_ptr< RiggedMesh > rig;
	AnimationGraph< Skeleton::BoneTransform > graph{
		[](auto const &a, auto const &, float) { return a; }};

	glm::vec2 velocity{0.0f, 0.0f};
	glm::quat base_rotation = glm::quat(1, 0, 0, 0);

	// civilian switches between walk/stand
	float move_timer = 0.0f;
	float pause_timer = 0.0f;

	glm::vec2 start_pos{0.0f, 0.0f};
	glm::vec2 walk_bounds{20.0f, 20.0f};

	float speed = 0.5f;
	float radius = 0.6f;

	std::mt19937 rng{std::random_device{}()};
	// --- NEW: attraction / pull-to-player state ---
	glm::vec3 pull_target{0.0f, 0.0f, 0.0f};
	bool being_pulled = false;
	float pull_speed = 3.0f; // faster than normal wandering
};

inline float rand(std::mt19937 &rng, float min, float max) {
	return min + (max - min) * (rng() >> 8) * (1.0f / 16777216.0f);
}

inline void civilian_update(Civilian &c, float elapsed) {
	if (c.being_pulled) {
		glm::vec3 pos3 = c.transform->position;
		glm::vec2 pos(pos3.x, pos3.y);

		glm::vec2 target_xy(c.pull_target.x, c.pull_target.y);
		glm::vec2 to_target = target_xy - pos;
		float dist = glm::length(to_target);

		// stop when close enough to player
		if (dist > 0.6f) {
			glm::vec2 dir = to_target / dist;
			glm::vec2 step = dir * c.pull_speed * elapsed;

			// move civilian on x,y plane
			c.transform->position.x += step.x;
			c.transform->position.y += step.y;

			// update velocity for rotation
			c.velocity = step / std::max(elapsed, 1e-4f);

			// rotate to face movement direction
			if (glm::length(c.velocity) > 1e-3f) {
				float angle = std::atan2(c.velocity.x, c.velocity.y);
				glm::quat target_rot = glm::angleAxis(angle, glm::vec3(0, 0, 1)) * c.base_rotation;
				c.transform->rotation = glm::slerp(
					c.transform->rotation,
					target_rot,
					1.0f - std::exp(-6.0f * elapsed)
				);
			}
		} else {
			// reached player: stop pulling, resume normal AI with a short pause
			c.being_pulled = false;
			c.velocity = glm::vec2(0.0f);
			c.pause_timer = rand(c.rng, 1.0f, 3.0f);
		}

		return; // skip normal wandering logic while being pulled
	}

	if (c.pause_timer > 0.0f) {
        // decrease resting time
		c.pause_timer -= elapsed;
		c.velocity *= std::max(0.0f, 1.0f - elapsed * 6.0f);
	} else {
		// if done walking, choose new walk direction
		if (c.move_timer <= 0.0f) {
			c.move_timer = rand(c.rng, 2.0f, 4.0f);
			float angle = rand(c.rng, -glm::pi< float >(), glm::pi< float >());
			c.velocity = glm::vec2(std::sin(angle), std::cos(angle)) * c.speed;
		}

		glm::vec2 pos(c.transform->position.x, c.transform->position.y);

        // if going out of bounds, switch direction
		glm::vec2 next = pos + c.velocity * elapsed;
		glm::vec2 min_bound = c.start_pos - c.walk_bounds;
		glm::vec2 max_bound = c.start_pos + c.walk_bounds;
		if (next.x < min_bound.x || next.x > max_bound.x) {
			c.velocity.x = -c.velocity.x;
		}
		if (next.y < min_bound.y || next.y > max_bound.y) {
			c.velocity.y = -c.velocity.y;
		}

		next = pos + c.velocity * elapsed;
		c.transform->position.x = next.x;
		c.transform->position.y = next.y;

		// rotate to direction
		if (glm::length(c.velocity) > 1e-3f) {
			float angle = std::atan2(c.velocity.x, c.velocity.y);
			glm::quat target = glm::angleAxis(angle, glm::vec3(0, 0, 1)) * c.base_rotation;
			c.transform->rotation = glm::slerp(c.transform->rotation, target, 1.0f - std::exp(-6.0f * elapsed));
		}

		c.move_timer -= elapsed;
		if (c.move_timer <= 0.0f) {
			c.pause_timer = rand(c.rng, 1.0f, 3.0f);
		}
	}
}

inline void civilian_avoid_obstacles(std::vector< Civilian > &civilians, const std::vector< std::pair< Scene::Transform *, float > > &obstacles) {
	// For each civilian, check against each obstacle
    for (auto &c : civilians) {
		glm::vec2 pos(c.transform->position.x, c.transform->position.y);

		for (auto &[obs_transform, obs_radius] : obstacles) {
			glm::vec2 obs_pos(obs_transform->position.x, obs_transform->position.y);
			glm::vec2 delta = pos - obs_pos;

			float dist = glm::length(delta);
			float min_dist = c.radius + obs_radius;
			if (dist < min_dist) {
				glm::vec2 dir = delta / dist;
				float move_amount = (min_dist - dist) + 0.1f;

				c.transform->position.x += dir.x * move_amount;
				c.transform->position.y += dir.y * move_amount;
				c.velocity += dir * 0.1f;

				c.move_timer = std::max(c.move_timer, 0.35f);
				pos = glm::vec2(c.transform->position.x, c.transform->position.y);
			}
		}
	}
}

inline void resolve_collisions(std::vector< Civilian > &civilians) {
	// For each civilian pair, check if they overlap
	for (size_t i = 0; i < civilians.size(); ++i) {
		Civilian &cA = civilians[i];
		for (size_t j = i + 1; j < civilians.size(); ++j) {
			Civilian &cB = civilians[j];

			glm::vec2 posA(cA.transform->position.x, cA.transform->position.y);
			glm::vec2 posB(cB.transform->position.x, cB.transform->position.y);

			glm::vec2 delta = posB - posA;
			float dist = glm::length(delta);

			float min_dist = cA.radius + cB.radius;
			if (dist < min_dist) {
				glm::vec2 dir = delta / dist;
				float move_amount = (min_dist - dist) * 0.5f + 0.1f;

				// Move civilians in opposite directions
				cA.transform->position.x -= dir.x * move_amount;
				cA.transform->position.y -= dir.y * move_amount;
				cB.transform->position.x += dir.x * move_amount;
				cB.transform->position.y += dir.y * move_amount;
				cA.velocity -= dir * 0.1f;
				cB.velocity += dir * 0.1f;

				// keep walking so they can stay apart
				cA.move_timer = std::max(cA.move_timer, 0.35f);
				cB.move_timer = std::max(cB.move_timer, 0.35f);
			}
		}
	}
}
