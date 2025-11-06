#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "Camera.hpp"

#include <glm/glm.hpp>
#include <random>
#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	Scene::Transform *player = nullptr;
	Scene::Transform *enemy = nullptr;
	Scene::Transform *final_deer = nullptr;
	Scene::Transform *final_deer_leg = nullptr;
	int deer_stage = 0; // 0 = original deer, 1 = deer + leg, 2 = ... etc.
	glm::quat player_base_rotation;
	
	//camera:
	Camera *cam;
	Scene::Camera *camera = nullptr;
	bool focus_mode = false;           // toggled with right mouse
	float player_speed_factor = 1.0f;  // 1.0 normally, 0.5 in focus mode
	float base_fovy = 1.0f;            // store original camera fovy
	float target_fovy = 1.0f;          // what fovy we’re moving toward
	float zoom_speed = 3.0f; 
	float stalk_charge = 0.0f;
	// rate per second:
	float stalk_charge_rate = 0.2f;   // fills while holding RMB
	// float stalk_decay_rate = 0.025f;    // drains when not holding
	bool  stalking = false;           // true while RMB is held
	bool enemy_visible = true; // updated in draw(), used in next update()
	bool enemy_on_screen = false;	// NEW: updated in update() via clip-space test

	//Excution mode
	bool execution_mode = false;        // Trigger
	bool enemy_alive = true;            // Detect is alive or not
	float execution_range = 10.0f;       // excution area

	// --- enemy patrol ---
	std::vector<glm::vec3> enemy_waypoints;
	size_t enemy_wp_idx = 0;
	float enemy_speed = 6.0f;           // units/sec
	float enemy_wait_timer = 0.0f;      // seconds left to wait at a waypoint
	float enemy_wait_at_point = 0.4f;   // pause duration
	float enemy_reach_epsilon = 0.15f;  // how close counts as "arrived"
	glm::quat enemy_base_rotation;      // remember original facing
	// Enemy vision
	bool  being_watched = false;   // updated in update(), read in draw()
	bool  watched_latched = false;
	float enemy_view_distance = 10.0f;  // max detection range (units)
	float enemy_fov_deg = 70.0f;        // vision cone (full angle)
	float watched_grace = 0.15f;      // seconds
	float watched_grace_timer = 0.0f; // countdown
	//game over set
	float watched_accum = 0.0f;          // continuous time (seconds) currently being watched
	float watch_to_gameover = 5.0f;      // threshold (seconds)
	bool  game_over = false;             // simple game-over latch

	void trigger_game_over();            // declare handler

	// Enemy collapse animation (after execution)
	bool enemy_collapsing = false;
	float enemy_collapse_t = 0.0f;
	float enemy_collapse_duration = 0.7f; // seconds
	glm::quat enemy_collapse_start;
	glm::quat enemy_collapse_end;
	//UI
	GLuint deer_ui_tex = 0;        // 
	glm::uvec2 deer_ui_size = glm::uvec2(0); //

	GLuint deer_ui_vao = 0;        // 
	GLuint deer_ui_vbo = 0;

	float deer_ui_target_width_px = 160.0f; //  128/160/192 
	// Kill count & Dash skill unlock
	int kill_count = 0;
	bool dash_skill = false;
	bool attraction_ability = false;  // unlocked after killing at least 2 enemies
	bool  dashing = false;
	float dash_timer = 0.0f;       // remaining dash time (sec)
	float dash_duration = 0.18f;   // how long a dash lasts
	float dash_cooldown_timer = 0.0f;
	float dash_cooldown = 0.8f;    // time before dash can be used again
	float dash_speed = 55.0f;      // units/sec while dashing
	glm::vec3 dash_dir = glm::vec3(0.0f); // world-space direction of dash
	//
	std::vector<Sound::Sample const *> attraction_sounds; // <-- use raw pointers
	std::mt19937 rng{123456u};       // simple RNG; you can seed with time if you want
	float attraction_cooldown_timer = 0.0f;
	float attraction_cooldown = 0.6f; // avoid accidental audio spam

};
