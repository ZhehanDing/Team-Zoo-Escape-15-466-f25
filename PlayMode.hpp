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

};
