#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include "Collision.hpp"
#include "Skeleton.hpp"
#include "Animation.hpp"

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
	} left, right, down, up, lshift, space, one, two;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// if time allows, potentially maintain structure to register and play sfx
	// std::map< std::string, std::vector< Sound::Sample *>> sfx;

	struct Player {
		Scene::Transform transform;
		std::vector< Scene::Drawable > drawables;

		Collider col = Collider(&transform);
	} player;

	//camera:
	Scene::Camera *camera = nullptr;
	struct CameraInfo {
		float yaw = 0.f;
		float pitch = glm::radians(90.f);
	} cam_info;
};
