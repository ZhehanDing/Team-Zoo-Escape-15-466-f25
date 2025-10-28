#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

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
	glm::quat player_base_rotation;
	
	//camera:
	Scene::Camera *camera = nullptr;
	bool focus_mode = false;           // toggled with right mouse
	float player_speed_factor = 1.0f;  // 1.0 normally, 0.5 in focus mode
	float base_fovy = 1.0f;            // store original camera fovy
	float target_fovy = 1.0f;          // what fovy we’re moving toward
	float zoom_speed = 3.0f; 
	float stalk_charge = 0.0f;
	// rate per second:
	float stalk_charge_rate = 0.05f;   // fills while holding RMB
	float stalk_decay_rate = 0.025f;    // drains when not holding
	bool  stalking = false;           // true while RMB is held
	bool enemy_visible = true; // updated in draw(), used in next update()
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
};
