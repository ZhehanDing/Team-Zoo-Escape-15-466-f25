#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <memory>
#include <map>
#include "Scene.hpp"
#include "Skeleton.hpp"
#include "RiggedMesh.hpp"
#include "Animation.hpp"

struct Civilian {
	enum State { STAND, WALK, RUN, BETWEEN };
	State state = STAND;
	bool bumped = false; // Whether player collided into civilian while standing

	Scene::Transform *transform = nullptr;
	std::unique_ptr< Skeleton > skel;
	std::unique_ptr< RiggedMesh > rig;

	std::unique_ptr< AnimationBuffer< Skeleton::BoneTransform > > anim_buffer;
	AnimationGraph< Skeleton::BoneTransform > graph{[](auto const &a, auto const &, float) { return a; }};

	glm::vec2 velocity{0.0f, 0.0f};
	glm::quat base_rotation = glm::quat(1, 0, 0, 0);

	// civilian switches between walk/stand
	float move_timer = 0.0f;
	float pause_timer = 0.0f;
	float transition_delay = 0.0f; // Random delay before allowing transitions to prevent synchronization
	float playback_speed = 1.0f; // Animation playback speed multiplier (0.8x to 1.2x) to differentiate civilians

	glm::vec2 start_pos{0.0f, 0.0f};
	glm::vec2 walk_bounds{20.0f, 20.0f};

	float speed = 0.2f;
	float radius = 0.6f;

	std::mt19937 rng;
};

inline float rand(std::mt19937 &rng, float min, float max) {
	return min + (max - min) * (rng() >> 8) * (1.0f / 16777216.0f);
}

inline void civilian_set_state(Civilian &c, Civilian::State next) {
	if (next == c.state) return;

	auto set_state = [&](const std::string &name) {
		auto it = c.graph.states.find(name);
		if (it != c.graph.states.end()) {
			c.graph.current_state = &it->second;
			c.graph.playback = 0.0f;
			c.graph.keyframe_index = 0;
		}
	};

	Civilian::State prev = c.state;

	if (prev == Civilian::STAND && next == Civilian::WALK) {
		set_state("StandToWalk");
		c.state = Civilian::BETWEEN;
		return;
	}
	if (prev == Civilian::WALK && next == Civilian::STAND) {
		set_state("WalkToStand");
		c.state = Civilian::BETWEEN;
		return;
	}
	if (prev == Civilian::WALK && next == Civilian::RUN) {
		set_state("WalkToRun");
		c.state = Civilian::BETWEEN;
		return;
	}
	if (prev == Civilian::RUN && next == Civilian::WALK) {
		set_state("RunToWalk");
		c.state = Civilian::BETWEEN;
		return;
	}

	if (next == Civilian::STAND) {
		set_state("Stand");
		c.state = Civilian::STAND;
	} else if (next == Civilian::WALK) {
		set_state("Walk");
		c.state = Civilian::WALK;
	} else if (next == Civilian::RUN) {
		set_state("Run");
		c.state = Civilian::RUN;
	}
}

inline void civilian_update(Civilian &c, float elapsed) {
	float prev_playback  = c.graph.playback;

	// Apply per-civilian playback speed to differentiate animations
	c.graph.update(elapsed * c.playback_speed);

	auto *cur_state = c.graph.current_state;
	float cur_playback = c.graph.playback;

	auto &states = c.graph.states;
	auto *stand_state = &states.find("Stand")->second;
	auto *walk_state = &states.find("Walk")->second;
	auto *run_state = &states.find("Run")->second;
	auto *stand_to_walk_state = &states.find("StandToWalk")->second;
	auto *walk_to_stand_state = &states.find("WalkToStand")->second;
	auto *walk_to_run_state = &states.find("WalkToRun")->second;
	auto *run_to_walk_state = &states.find("RunToWalk")->second;

	bool finished_transition = false;
	Civilian::State effective_state = c.state;

	if (c.state == Civilian::BETWEEN) {
		if (cur_state == stand_to_walk_state) {
			effective_state = Civilian::WALK;
		} else if (cur_state == walk_to_stand_state) {
			effective_state = Civilian::STAND;
		} else if (cur_state == walk_to_run_state) {
			effective_state = Civilian::RUN;
		} else if (cur_state == run_to_walk_state) {
			effective_state = Civilian::WALK;
		}
		
		// If transition animation has finished
		float anim_length = cur_state->animation.get_anim_length();
		const float epsilon = 0.001f;
		if (cur_playback >= anim_length - epsilon) {
			if (cur_state == stand_to_walk_state) {
				c.graph.current_state = walk_state;
				c.state = Civilian::WALK;
			} else if (cur_state == walk_to_stand_state) {
				c.graph.current_state = stand_state;
				c.state = Civilian::STAND;
			} else if (cur_state == walk_to_run_state) {
				c.graph.current_state = run_state;
				c.state = Civilian::RUN;
			} else if (cur_state == run_to_walk_state) {
				c.graph.current_state = walk_state;
				c.state = Civilian::WALK;
			} else {
				printf("Error, could not find transition state.\n");
			}
			c.graph.playback = 0.0f;
			c.graph.keyframe_index = 0;
			finished_transition = true;
			effective_state = c.state;
		}
	}

	// Otherwise, detect if we've finished a non-transitional animation
	bool finished_loop = false;
	if (!finished_transition && c.state != Civilian::BETWEEN && cur_state->animation.loop) {
		float anim_length = cur_state->animation.get_anim_length();
		// If we're at the first or last 5% of the animation
		if (prev_playback >= anim_length * 0.95f && cur_playback <= anim_length * 0.05f) {
			finished_loop = true;
		}
	}

	// Update transition delay timer
	if (c.transition_delay > 0.0f) {
		c.transition_delay = std::max(0.0f, c.transition_delay - elapsed);
	}

	// Only allow transitions if delay has passed
	if (finished_loop && c.transition_delay <= 0.0f) {
		Civilian::State current_state = c.state;
		Civilian::State next = current_state;

		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		float r = dist(c.rng);

		if (current_state == Civilian::STAND) {
			if (c.bumped) {
				next = Civilian::WALK;
				c.bumped = false;
			} else {
				if (r < 0.3f) {
					next = Civilian::WALK;
				}
			}
		} else if (current_state == Civilian::WALK) {
			if (r < 0.5f) {
				next = Civilian::STAND;
			} else if (r < 0.9f) {
				next = Civilian::WALK;
			} else {
				next = Civilian::RUN;
			}
		} else if (current_state == Civilian::RUN) {
			if (r < 0.8f) {
				next = Civilian::WALK;
			} else {
				next = Civilian::RUN;
			}
		}

		if (next != current_state) {
			civilian_set_state(c, next);
			c.transition_delay = rand(c.rng, 0.1f, 0.5f);
		} else {
			c.transition_delay = rand(c.rng, 0.05f, 0.2f);
		}
	}
	
	c.rig->update(elapsed);

	// Movement amount based on animation type
	glm::vec2 pos(c.transform->position.x, c.transform->position.y);
	glm::vec2 min_bound = c.start_pos - c.walk_bounds;
	glm::vec2 max_bound = c.start_pos + c.walk_bounds;

	float move_speed = 0.0f;
	if (effective_state == Civilian::WALK) {
		move_speed = c.speed;
	} else if (effective_state == Civilian::RUN) {
		move_speed = c.speed * 3.0f;
	}

	glm::mat4x3 frame = c.transform->make_parent_from_local();
	glm::vec3 forward_dir = frame[1];
	glm::vec2 forward(-forward_dir.x, -forward_dir.y);
	glm::vec2 next = pos + forward * move_speed * 3.0f * elapsed;

	// Check bounds and turn if at edge
	if (next.x < min_bound.x || next.x > max_bound.x || next.y < min_bound.y || next.y > max_bound.y) {
		c.transform->rotation = c.transform->rotation * glm::angleAxis(3.14f, glm::vec3(0, 0, 1));
		frame = c.transform->make_parent_from_local();
		forward_dir = frame[1];
		forward = glm::vec2(-forward_dir.x, -forward_dir.y);
		next = pos + forward * move_speed * 3.0f * elapsed;
	}

	if (next.x < min_bound.x) next.x = min_bound.x;
	if (next.x > max_bound.x) next.x = max_bound.x;
	if (next.y < min_bound.y) next.y = min_bound.y;
	if (next.y > max_bound.y) next.y = max_bound.y;

	c.transform->position.x = next.x;
	c.transform->position.y = next.y;
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

				if (c.state == Civilian::STAND) {
					c.bumped = true;
				}
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

				if (cA.state == Civilian::STAND) cA.bumped = true;
				if (cB.state == Civilian::STAND) cB.bumped = true;			
			}
		}
	}
}
