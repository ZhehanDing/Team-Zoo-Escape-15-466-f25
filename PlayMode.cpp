#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "RiggedMesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint meshes_for_lit_color_texture_program = 0;
GLuint joints_meshes_for_lit_color_texture_program = 0;
GLuint surface_meshes_for_lit_color_texture_program = 0;

/*
Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("test.pnct"));
	meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});
*/

Load< SkeletonBuffer > skeletons(LoadTagDefault, []() -> SkeletonBuffer const * {
	SkeletonBuffer const *ret = new SkeletonBuffer(data_path("../animations/test.skel"));
	return ret;
});

Load< AnimationBuffer< Skeleton::BoneTransform > > animations(LoadTagDefault, []() -> AnimationBuffer< Skeleton::BoneTransform > const * {
	AnimationBuffer< Skeleton::BoneTransform > const *ret = new AnimationBuffer< Skeleton::BoneTransform >(data_path("../animations/test.anim"));
	return ret;
});

DynamicMeshBuffer *joints;
DynamicMeshBuffer *surface;
Load< RiggedMeshBuffer > rigged_meshes(LoadTagDefault, []() -> RiggedMeshBuffer const * {
	RiggedMeshBuffer const *ret = new RiggedMeshBuffer(data_path("test.pnct"));
	joints = new DynamicMeshBuffer();
	surface = new DynamicMeshBuffer();
	joints_meshes_for_lit_color_texture_program = joints->make_vao_for_program(lit_color_texture_program->program);
	surface_meshes_for_lit_color_texture_program = surface->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Scene::Transform *base = nullptr;
Load< Scene > test_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("test.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		RiggedMesh const &mesh = rigged_meshes->lookup(mesh_name);
		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		if (mesh_name == "Beta_Surface") {
			base = transform;
		}
		if (scene.drawables.size() == 2) {
			Scene::Drawable &front = scene.drawables.front();
			Scene::Drawable &back = scene.drawables.back();

			if (mesh_name == "Beta_Surface") {
				front.transform->parent = back.transform;
			}
			else {
				back.transform->parent = front.transform;
			}
			
		}
		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = mesh_name == "Beta_Joints" ? joints_meshes_for_lit_color_texture_program : surface_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = GL_TRIANGLES;
		drawable.pipeline.start = 0;
		drawable.pipeline.count = (GLuint) mesh.vertices.size();
	});
});

std::function< glm::vec3(const glm::vec3 &, const glm::vec3 &, float) > f = [](const glm::vec3 &a, const glm::vec3 &b, float t) {
	return a * (1 - t) + b * t;
};
AnimationGraph < glm::vec3 > graph = AnimationGraph< glm::vec3 >(f);
Animation< glm::vec3 > move_x_anim("MoveX", 24, true);
Animation< glm::vec3 > move_y_anim("MoveY", 24, true);

auto g = [](const Skeleton::BoneTransform &a, const Skeleton::BoneTransform &b, float t) {
	Skeleton::BoneTransform transform;
	transform.position = glm::mix(a.position, b.position, t);
	transform.rotation = glm::slerp(a.rotation, b.rotation, t);
	transform.scale = glm::mix(a.scale, b.scale, t);
	Skeleton::BoneTransform c(a);
	return c;
};

AnimationGraph < Skeleton::BoneTransform > rig_graph = AnimationGraph< Skeleton::BoneTransform >(g);


const Skeleton *skeleton;
PlayMode::PlayMode() : scene(*test_scene) {
	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	{
		player.transform.position = camera->transform->position;
		camera->transform->parent = &player.transform;
	}

	move_x_anim.add_keyframes({ glm::vec3(-10.f, 0.f, 0.f), glm::vec3(10.f, 0.f, 0.f), glm::vec3(-10.f, 0.f, 0.f) }, { 0.f, 5.f, 10.f }, { "position" });
	move_y_anim.add_keyframes({ glm::vec3(0.f, -10.f, 0.f), glm::vec3(0.f, 10.f, 0.f), glm::vec3(0.f, -10.f, 0.f) }, { 0.f, 5.f, 10.f }, { "position" });
	graph.add_state(move_x_anim);
	graph.add_state(move_y_anim);

	graph.add_transition("MoveX", std::make_pair([&](AnimationGraph< glm::vec3 > &G) {
		return one.pressed != 0;
	}, "MoveY"));

	graph.add_transition("MoveY", std::make_pair([&](AnimationGraph< glm::vec3 > &G) {
		return two.pressed != 0;
	}, "MoveX"));

	rig_graph.add_state(animations->lookup("Dance"));
	rig_graph.add_state(animations->lookup("Dance1"));
	rig_graph.add_transition("Dance", std::make_pair([&](AnimationGraph< Skeleton::BoneTransform > &G) {
		return one.pressed != 0;
	}, "Dance1"));

	rig_graph.add_transition("Dance1", std::make_pair([&](AnimationGraph< Skeleton::BoneTransform > &G) {
		return two.pressed != 0;
	}, "Dance"));

	skeleton = &skeletons->lookup("Armature");
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_LSHIFT) {
			lshift.downs += 1;
			lshift.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			space.downs = 1;
			space.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_1) {
			one.downs = 1;
			one.pressed = true;
			return true;
		} 
		else if (evt.key.key == SDLK_2) {
			two.downs = 1;
			two.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_R) {
			Sound::stop_all_samples();
			return false;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_LSHIFT) {
			lshift.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			space.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_1) {
			one.pressed = false;
			return true;
		} 
		else if (evt.key.key == SDLK_2) {
			two.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == false) {
			SDL_SetWindowRelativeMouseMode(Mode::window, true);
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);

			// yaw and pitch to avoid rolling, clamp each
			cam_info.yaw -= motion.x * camera->fovy;
			if (cam_info.yaw > (float)M_PI) cam_info.yaw -= (float)M_PI * 2.f;
			else if (cam_info.yaw < -(float)M_PI) cam_info.yaw += (float)M_PI * 2.f;

			cam_info.pitch += motion.y * camera->fovy;
			if (cam_info.pitch > (float)M_PI) cam_info.pitch = (float)M_PI;
			else if (cam_info.pitch < 0.f) cam_info.pitch = 0.f;

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	auto rig_res = rig_graph.sample(); 
	auto pose = skeleton->pose(rig_res);
	auto vertices_surface = skeleton->skin(pose, rigged_meshes->lookup("Beta_Surface"));
	auto vertices_joints = skeleton->skin(pose, rigged_meshes->lookup("Beta_Joints"));
	surface->set(vertices_surface, GL_DYNAMIC_DRAW);
	joints->set(vertices_joints, GL_DYNAMIC_DRAW);
 	rig_graph.update(elapsed);

	auto sampled_position = graph.sample();
	base->position = sampled_position.at("position");
	graph.update(elapsed);

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//move camera:
	float cos_yaw = std::cosf(cam_info.yaw);
	float sin_yaw = std::sinf(cam_info.yaw);
	/*
	float cos_pitch = std::cosf(cam_info.pitch);
	float sin_pitch = std::sinf(cam_info.pitch);
	*/
	camera->transform->rotation = glm::quat( glm::vec3(cam_info.pitch, 0.0f, cam_info.yaw) );

	// handle player movement
	// player's ability to control movement is impaired by siren
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 5.0f;
		glm::vec3 move = glm::vec3(0.0f);
		glm::vec3 player_dir = glm::vec3(0.f);

		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;
		if (lshift.pressed && !space.pressed) move.z =-1.0f;
		if (!lshift.pressed && space.pressed) move.z = 1.0f;
		float dist = PlayerSpeed * elapsed;

		if (move != glm::vec3(0.f)) {
			//make it so that moving diagonally doesn't go faster:
			player_dir = glm::normalize(glm::vec3(
				move.x * cos_yaw - move.y * sin_yaw, 
				move.x * sin_yaw + move.y * cos_yaw, 
				move.z
			));
		}

		player.transform.position += player_dir * dist;
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	lshift.downs = 0;
	space.downs = 0;
	one.downs = 0;
	two.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.f, 1.f, 1.0f)));
	glUseProgram(0);

	glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.
	
	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));
/*
		constexpr float H = 0.09f;
		lines.draw_text("Find the code for the correct lever configuration to repair the oil rig! Beware of the siren's song!",
			glm::vec3(-aspect + 0.1f * H, -.98 + 0.1f * H, 0.0),
			glm::vec3(H * .9f, 0.0f, 0.0f), glm::vec3(0.0f, H  * .9f, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Find the code for the correct lever configuration to repair the oil rig! Beware of the siren's song!",
			glm::vec3(-aspect + 0.1f * H + ofs, -.98 + 0.1f * H + ofs, 0.0),
			glm::vec3(H * .9f, 0.0f, 0.0f), glm::vec3(0.0f, H * .9f, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
*/
/*
		// world drawing for physics debugging.
		DrawLines world(glm::mat3x4(camera->make_projection()) * camera->transform->make_local_from_world());
		for (Collider *col : colliders) {
			world.draw_box(col->get_transformation_matrix(), glm::u8vec4(255, 0, 0, 255));
		}
*/
	}
	GL_ERRORS();
}