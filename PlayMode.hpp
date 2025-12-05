#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "Camera.hpp"

#include <glm/glm.hpp>
#include <random>
#include <vector>
#include <deque>


#include "Animation.hpp"
#include "Mode.hpp"
#include "RiggedMesh.hpp"
#include "Skeleton.hpp"
#include <deque>
#include <vector>
#include "Civilian.hpp" 

#include "Particles.hpp" 
#include "Sampler.hpp" 

#include "Overlay.hpp"

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	// functions called by main loop:
	virtual bool handle_event(SDL_Event const &,
							  glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	// input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	// local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	Overlay overlay;

	std::vector<Civilian> civilians;
	std::vector< Scene::Drawable * > civilian_drawables;
	Scene::Transform *execution_target = nullptr;
	Scene::Transform *stalk_target = nullptr;
	
	Scene::Transform *player = nullptr;
	Scene::Transform *final_deer = nullptr;
	Scene::Transform *final_deer_leg = nullptr;
	Scene::Transform *sky = nullptr;
	Scene::Transform *gate = nullptr;
	Scene::Transform *gate_L = nullptr;
	Scene::Transform *gate_R = nullptr;
	Scene::Transform *gate_collider = nullptr;
	Scene::Transform *deer_fence_collider = nullptr;
	Scene::Transform *zoo_fence_near_collider = nullptr;
	Scene::Transform *zoo_fence_far_collider = nullptr;
	std::vector<Scene::Transform *> trees;
	// std::vector<Scene::Transform *> cylinders;
	int deer_stage = 0; // 0 = original deer, 1 = deer + leg, 2 = ... etc.
	glm::quat player_base_rotation;

	// Deer Human
	std::unique_ptr< Skeleton > deer_human_skeleton;
	std::vector< std::unique_ptr< RiggedMesh > > deer_human_rigs;
	std::vector< Scene::Drawable * > deer_human_drawables;
	std::vector< uint32_t > deer_human_original_counts;
	AnimationGraph< Skeleton::BoneTransform > deer_human_graph =
		AnimationGraph< Skeleton::BoneTransform >(
			[](Skeleton::BoneTransform const &a,
			   Skeleton::BoneTransform const &,
			   float) { return a; });
	bool deer_human_moving = false;
	bool is_deer_human = false;

	// Gate
	void trigger_gate_open();
	bool gate_can_open = false;
	bool gate_anim_playing = false;
	float gate_rot_t = 0.0f;
	float gate_rot_duration_1 = 6.0f;
	float gate_rot_duration_2 = 10.0f;

	glm::quat gate_L_start, gate_L_end;
	glm::quat gate_R_start, gate_R_end;
	glm::quat gate_L_final, gate_R_final;

	// camera:
	Camera *cam = nullptr;
	Scene::Camera *camera = nullptr;
	bool focus_mode = false;		  // toggled with right mouse
	float player_speed_factor = 0.5f; // 1.0 normally, 0.5 in focus mode
	float base_fovy = 1.0f;			  // store original camera fovy
	float target_fovy = 1.0f;		  // what fovy we’re moving toward
	float zoom_speed = 3.0f;
	float stalk_charge = 0.0f;
	// rate per second:
	float stalk_charge_rate = 0.2f;   // fills while holding RMB
	// float stalk_decay_rate = 0.025f;    // drains when not holding
	bool stalking = false;           // true while RMB is held
	bool enemy_visible = true; // updated in draw(), used in next update()
	bool enemy_on_screen = false;	// NEW: updated in update() via clip-space test
	glm::vec2 stalk_target_ndc;
	float maximum_stalk_dist = 50.f;

	//Excution mode
	bool execution_mode = false;        // Trigger
	float execution_range = 3.0f;       // excution area
	glm::vec2 execution_target_ndc;
	// Enemy vision
	bool being_watched = false; // updated in update(), read in draw()
	Civilian *watching_civilian = nullptr;
	bool watching_on_screen = false;
	glm::vec2 watching_civ_ndc;
	bool watched_latched = false;
	float enemy_view_distance = 10.0f; // max detection range (units)
	float enemy_fov_deg = 70.0f;	   // vision cone (full angle)
	float watched_grace = 0.15f;	   // seconds
	float watched_grace_timer = 0.0f;  // countdown
	// game over set
	float watched_accum =
		0.0f;						// continuous time (seconds) currently being watched
	float watch_to_gameover = 3.0f; // threshold (seconds)
	bool game_over = false;			// simple game-over latch

	void trigger_game_over();            // declare handler

	bool game_success = false;
	void trigger_game_success();
	//11/24 update
	// --- high-level game screen state ---
	enum class ScreenState {
		MENU,      // main menu (start of game)
		PLAYING,   // normal gameplay
	};

	ScreenState screen_state = ScreenState::PLAYING;
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
	std::vector<int> attraction_ids = {1, 2, 3, 4};
	std::mt19937 rng{123456u};       // simple RNG; you can seed with time if you want
	float attraction_cooldown_timer = 0.0f;
	float attraction_cooldown = 0.6f; // avoid accidental audio spam

	ParticleGenerator blood_pg = ParticleGenerator(Sphere::sample);
	ParticleGenerator dust_pg = ParticleGenerator(Sphere::sample);

	// Sound
	std::shared_ptr<Sound::PlayingSample> bg_loop;
	std::shared_ptr<Sound::PlayingSample> footstep_loop = nullptr;
	std::shared_ptr<Sound::PlayingSample> stalking_loop = nullptr;
	std::shared_ptr<Sound::PlayingSample> watched_playing = nullptr;
	void start_footstep_loop();
	void stop_footstep_loop();
	void start_stalking_loop();
	void stop_stalking_loop();

	//11/24 Update Alex Ding
	// UI hint for dash unlock
	bool dash_hint_active = false;
	float dash_hint_timer = 0.0f;   // seconds
	bool sound_hint_active =false;
	float sound_hint_timer = 0.0f;   // seconds
	float watched_tooltip_timer = 0.0f;   // seconds, for pulsing effect
	bool pass_hint_active =false;
	float pass_hint_timer = 5.0f;
};
