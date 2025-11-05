#include "PlayMode.hpp"
#include <algorithm>
#include <cmath>
// #include "LitColorTextureProgram.hpp"
#include "BasicMaterialDeferredProgram.hpp"
#include "LightMeshes.hpp"
#include "CopyToScreenProgram.hpp"

#include "Animation.hpp"
#include "DrawLines.hpp"
#include "Load.hpp"
#include "Mesh.hpp"
#include "RiggedMesh.hpp"
#include "Skeleton.hpp"
#include "SkinningProgram.hpp"
#include "gl_errors.hpp"
#include "load_save_png.hpp"
#include "data_path.hpp"
#include "check_fb.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint zoo_for_basic_material_deferred_object = 0;
GLuint light_for_basic_material_deferred_light = 0;
// GLuint zoo_meshes_for_lit_color_texture_program = 0;

Load< MeshBuffer > zoo_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("zoo_nolink.pnct"));
	zoo_for_basic_material_deferred_object = ret->make_vao_for_program(basic_material_deferred_object_program->program);
	return ret;
});
Load< Sound::Sample > attraction_voice_1(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Sound1.wav"));
});
Load< Sound::Sample > attraction_voice_2(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Sound2.wav"));
});
Load< Sound::Sample > attraction_voice_3(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Sound3.wav"));
});
Load< Sound::Sample > attraction_voice_4(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Sound4.wav"));
});

Load< Scene > zoo_scene_deferred(LoadTagDefault, []() -> Scene const * {
	light_for_basic_material_deferred_light = light_meshes->make_vao_for_program(basic_material_deferred_light_program->program);
	zoo_for_basic_material_deferred_object = zoo_meshes->make_vao_for_program(basic_material_deferred_object_program->program);

	Scene *ret = new Scene(data_path("zoo_nolink.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = zoo_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		// drawable.pipeline = lit_color_texture_program_pipeline;
		drawable.pipeline = basic_material_deferred_object_program_pipeline;

		drawable.pipeline.vao = zoo_for_basic_material_deferred_object;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

		float roughness = 1.0f;
		if (transform->name.substr(0, 9) == "Icosphere") { //TODO: change name
			roughness = (transform->position.y + 10.0f) / 18.0f;
		}
		drawable.pipeline.set_uniforms = [roughness](){
			glUniform1f(basic_material_deferred_object_program->ROUGHNESS_float, roughness);
		};
	});

	return ret;
});

//Helper: maintain a framebuffer to hold rendered geometry
struct FB {
	//object data gets stored in these textures:
	GLuint position_tex = 0;
	GLuint normal_roughness_tex = 0;
	GLuint albedo_tex = 0;
	
	//output image gets written to this texture:
	GLuint output_tex = 0;

	//depth buffer is shared between objects + lights pass:
	GLuint depth_rb = 0;

	GLuint objects_fb = 0; //(position, normal, albedo) + depth
	GLuint lights_fb = 0; //(output) + depth

	glm::uvec2 size = glm::uvec2(0);

	void resize(glm::uvec2 const &drawable_size) {
		if (drawable_size == size) return;
		size = drawable_size;

		//helper to allocate a texture:
		auto alloc_tex = [&](GLuint &tex, GLenum internal_format) {
			if (tex == 0) glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_2D, tex);
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		};

		//set up position_tex as a 32-bit floating point RGB texture:
		alloc_tex(position_tex, GL_RGB32F);

		//set up normal_roughness_tex as a 16-bit floating point RGBA texture:
		alloc_tex(normal_roughness_tex, GL_RGBA16F);

		//set up albedo_tex as an 8-bit fixed point RGBA texture:
		alloc_tex(albedo_tex, GL_RGBA8);

		//set up output_tex as an 8-bit fixed point RGBA texture:
		alloc_tex(output_tex, GL_RGBA8);

		//if depth_rb does not have a name, name it:
		if (depth_rb == 0) glGenRenderbuffers(1, &depth_rb);
		//set up depth_rb as a 24-bit fixed-point depth buffer:
		glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		//if objects framebuffer doesn't have a name, name it and attach textures:
		if (objects_fb == 0) {
			glGenFramebuffers(1, &objects_fb);
			//set up framebuffer: (don't need to do when resizing)
			glBindFramebuffer(GL_FRAMEBUFFER, objects_fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, position_tex, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normal_roughness_tex, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, albedo_tex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
			GLenum bufs[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
			glDrawBuffers(3, bufs);
			check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		//if lights-drawing framebuffer doesn't have a name, name it and attach textures:
		if (lights_fb == 0) {
			glGenFramebuffers(1, &lights_fb);
			//set up framebuffer: (don't need to do when resizing)
			glBindFramebuffer(GL_FRAMEBUFFER, lights_fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_tex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
			GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
			glDrawBuffers(1, bufs);
			check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

	}
} fb;


PlayMode::PlayMode() : scene(*zoo_scene_deferred) {
	//get pointers to transforms for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Player") player = &transform;
		if (transform.name == "Human.rigify") enemy = &transform;
		if (transform.name == "Final_Deer") final_deer = &transform;
		if (transform.name == "Final_Deer Leg") {
			final_deer_leg = &transform;
			transform.scale = glm::vec3(0.0f); // set invisible initially
		}
		if (transform.name == "Sky") sky = &transform;
		if (transform.name == "Gate") gate = &transform;
		if (transform.name.rfind("Fence", 0) == 0)
		{ // prefix match
			fences.push_back(&transform);
			// printf("Found fence: %s\n", transform.name.c_str());
		}
	}
	if (player == nullptr) throw std::runtime_error("Player not found.");
	if (enemy == nullptr) throw std::runtime_error("enemy not found.");
	if (final_deer == nullptr) throw std::runtime_error("final_deer not found.");
	if (final_deer_leg == nullptr) throw std::runtime_error("final_deer_leg not found.");
	if (sky == nullptr) throw std::runtime_error("sky not found.");
	if (gate == nullptr) throw std::runtime_error("gate not found.");

	player_base_rotation = player->rotation;

	for (auto &d : scene.drawables) {
		if (d.transform == enemy &&
			d.pipeline.vao == zoo_meshes_for_lit_color_texture_program) {
			d.pipeline.count = 0; // hide old enemy mesh
		}
	}

	Mesh const &human_mesh = human_meshes->lookup("base.001");
	Skeleton const &human_skel = human_skeletons->lookup("Human.rigify");

	enemy_skeleton = std::make_unique< Skeleton >(human_skel);
	auto g = [](const Skeleton::BoneTransform &a,
				const Skeleton::BoneTransform &b,
				float t) {
		Skeleton::BoneTransform out;
		out.position = glm::mix(a.position, b.position, t);
		out.rotation = glm::normalize(glm::slerp(a.rotation, b.rotation, t));
		out.scale = glm::mix(a.scale, b.scale, t);
		return out;
	};
	enemy_graph = AnimationGraph< Skeleton::BoneTransform >(g);
	enemy_graph.add_state(human_animations->lookup("Walk"));
	enemy_rig = std::make_unique< RiggedMesh >(
		human_meshes->buffer, human_infls->buffer, human_mesh, *enemy_skeleton, &enemy_graph);

	auto make_civilian = [&](const glm::vec3 &location) {
		Civilian c;

		scene.transforms.emplace_back();

		c.transform = &scene.transforms.back();
		c.transform->position = location;
		c.transform->rotation = enemy_base_rotation;
		c.base_rotation = c.transform->rotation;
		c.start_pos = glm::vec2(location.x, location.y);

		Mesh const &mesh = human_meshes->lookup("base.001");
		Skeleton const &sk = human_skeletons->lookup("Human.rigify");
		c.skel = std::make_unique< Skeleton >(sk);

		// c.graph.add_state(human_animations->lookup("Walk"));

		c.rig = std::make_unique< RiggedMesh >(
			human_meshes->buffer, human_infls->buffer, mesh, *c.skel, &c.graph);

		scene.drawables.emplace_back(c.transform);
		Scene::Drawable &drawable = scene.drawables.back();
		drawable.pipeline = skinning_program_pipeline;
		drawable.pipeline.vao = c.rig->make_vao_for_program(skinning_program->program);
		drawable.pipeline.type = c.rig->mesh.type;
		drawable.pipeline.start = c.rig->mesh.start;
		drawable.pipeline.count = c.rig->mesh.count;

		civilians.emplace_back(std::move(c));
	};

	// populate civilians
	std::mt19937 civilians_rng{std::random_device{}()};
	glm::vec3 center = glm::vec3(-40.0f, -30.0f, 0.0f);
	for (int i = 0; i < 4; i++) {
		float x = rand(civilians_rng, -10.0f, 10.0f);
		float y = rand(civilians_rng, -10.0f, 10.0f);

		make_civilian(center + glm::vec3(x, y, 0.0f));
	}
	center = glm::vec3(-5.0f, 0.0f, 0.0f);
	for (int i = 0; i < 4; i++) {
		float x = rand(civilians_rng, -10.0f, 10.0f);
		float y = rand(civilians_rng, -10.0f, 10.0f);

		make_civilian(center + glm::vec3(x, y, 0.0f));
	}
		center = glm::vec3(0.0f, 40.0f, 0.0f);
	for (int i = 0; i < 4; i++) {
		float x = rand(civilians_rng, -10.0f, 10.0f);
		float y = rand(civilians_rng, -10.0f, 10.0f);

		make_civilian(center + glm::vec3(x, y, 0.0f));
	}

	// populate rigged mesh
	scene.drawables.emplace_back(enemy);
	Scene::Drawable &enemy_drawable = scene.drawables.back();
	enemy_drawable.pipeline = skinning_program_pipeline;
	enemy_drawable.pipeline.vao =
		enemy_rig->make_vao_for_program(skinning_program->program);
	enemy_drawable.pipeline.type = enemy_rig->mesh.type;
	enemy_drawable.pipeline.start = enemy_rig->mesh.start;
	enemy_drawable.pipeline.count = enemy_rig->mesh.count;

	// get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	base_fovy = camera->fovy;

	cam = new Camera(camera, player);
	cam->set_orbit_offset_from_anchor(glm::vec3(0.f, 0.f, 2.f));
	cam->set_initial_look_degrees(-90.f, 180.f, 0.f); /* initial camera look: pitch, roll, yaw */
	cam->set_sensitivity(1.5f);
	cam->set_max_distance_from_camera_center(5.f);
	// cam->set_pitch_range(-(float)M_PI, 0.f); //default

	enemy_base_rotation = enemy->rotation;

	// build a simple square/loop around the enemy's start position:
	glm::vec3 e0 = enemy->position;
	float R = 6.0f; // patrol radius
	enemy_waypoints = {
		e0 + glm::vec3(0.0f, R, 0.0f),
		e0 + glm::vec3(R, 0.0f, 0.0f),
		e0 + glm::vec3(0.0f, -R, 0.0f),
		e0 + glm::vec3(-R, 0.0f, 0.0f)};
	enemy_wp_idx = 0;
	enemy_wait_timer = 0.0f;
	/*
	{
		std::vector<glm::u8vec4> pixels;
		glm::uvec2 size(0);

		// 把 deer UI.png 放到可被 data_path() 找到的位置（和 .scene/.pnct 同级 assets）
		// 如果你要临时从绝对路径读，也可直接写全路径。
		load_png(data_path("deer UI.png"), &size, &pixels, LowerLeftOrigin);
		deer_ui_size = size; // 记录像素尺寸

		glGenTextures(1, &deer_ui_tex);
		glBindTexture(GL_TEXTURE_2D, deer_ui_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // UI 不用 mipmap
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
			int(size.x), int(size.y), 0,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// ---------- NEW: 为屏幕空间四边形准备 VAO/VBO ----------
	{
		glGenVertexArrays(1, &deer_ui_vao);
		glGenBuffers(1, &deer_ui_vbo);

		glBindVertexArray(deer_ui_vao);
		glBindBuffer(GL_ARRAY_BUFFER, deer_ui_vbo);

		// 顶点格式：pos(x,y), uv(u,v) — 4 个 float
		// 先占位 6 个顶点（两个三角形），每帧只更新数据
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

		// 取用 LitColorTextureProgram 的属性位置：
		GLint pos_loc = glGetAttribLocation(lit_color_texture_program->program, "Position");
		GLint uv_loc  = glGetAttribLocation(lit_color_texture_program->program, "TexCoord");

		// 有些版本 Position 是 vec4，这里以 vec2 启用即可（剩余分量由 shader 补 0/1）
		glEnableVertexAttribArray(GLuint(pos_loc));
		glVertexAttribPointer(GLuint(pos_loc), 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (GLvoid*)0);

		glEnableVertexAttribArray(GLuint(uv_loc));
		glVertexAttribPointer(GLuint(uv_loc), 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (GLvoid*)(sizeof(float)*2));

		// 给 Normal/Color 设置常量值，避免 shader 里参与光照时报 0：
		GLint n_loc = glGetAttribLocation(lit_color_texture_program->program, "Normal");
		if (n_loc >= 0) { glDisableVertexAttribArray(GLuint(n_loc)); glVertexAttrib3f(GLuint(n_loc), 0.f, 0.f, 1.f); }
		GLint c_loc = glGetAttribLocation(lit_color_texture_program->program, "Color");
		if (c_loc >= 0) { glDisableVertexAttribArray(GLuint(c_loc)); glVertexAttrib3f(GLuint(c_loc), 1.f, 1.f, 1.f); }

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}*/
}

void PlayMode::trigger_game_over() {
	if (game_over) return; // idempotent
	game_over = true;
}

PlayMode::~PlayMode() {
	/*	// NEW: 释放 deer UI 资源
	if (deer_ui_tex) glDeleteTextures(1, &deer_ui_tex);
	if (deer_ui_vbo) glDeleteBuffers(1, &deer_ui_vbo);
	if (deer_ui_vao) glDeleteVertexArrays(1, &deer_ui_vao);
	*/
	attraction_sounds = {
		attraction_voice_1,  // implicit convert to Sound::Sample const *
		attraction_voice_2,
		attraction_voice_3,
		attraction_voice_4
	};

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
		// } else if (evt.key.key == SDLK_LCTRL) {
		// 	if (stalk_charge >= 1.0f && enemy_alive) {
        //     execution_mode = true;
        //     return true;
        // 	}
		}else if (evt.key.key == SDLK_SPACE) {
			// Start dash if unlocked, not already dashing, and off cooldown
			if (dash_skill && !dashing && dash_cooldown_timer <= 0.0f && !game_over) {
				// Dash direction: camera-forward (on ground plane)
				glm::mat4x3 cam_frame = player->make_parent_from_local();
				glm::vec3 frame_forward = -cam_frame[1];       // consistent with your WASD forward
				frame_forward.z = 0.0f;
				if (glm::dot(frame_forward, frame_forward) < 1e-6f) frame_forward = glm::vec3(0.0f, 1.0f, 0.0f);
				dash_dir = glm::normalize(frame_forward);

				dashing = true;
				dash_timer = dash_duration;
				dash_cooldown_timer = dash_cooldown;

				// Optional: slight FOV punch-in while dashing
				target_fovy = base_fovy * 2.1f;
			}
			return true;
		}else if (evt.key.key == SDLK_G) {
			if (attraction_ability && attraction_cooldown_timer <= 0.0f && !game_over) {
				if (!attraction_sounds.empty()) {
					std::uniform_int_distribution<int> dist(0, int(attraction_sounds.size()) - 1);
					int idx = dist(rng);

					// Play voice sound (non-3D version)
					Sound::play(*attraction_sounds[idx], 1.0f, 1.0f);

					// Optional 3D positional version:
					// glm::vec3 p = player->make_world_from_local()[3];
					// Sound::play_3D(*attraction_sounds[idx], p, 1.0f, 1.0f);

					attraction_cooldown_timer = attraction_cooldown;
				}
			}
			return true;
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
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == false) {
			SDL_SetWindowRelativeMouseMode(Mode::window, true);
			return true;
		}
		if (evt.button.button == SDL_BUTTON_RIGHT) {
			// Toggle ON:
			focus_mode = true;
			stalking = true;
			player_speed_factor = 0.2f;		// slow to 50%
			target_fovy = base_fovy * 0.7f; // zoom in a bit
			return true;
		} else if (evt.button.button == SDL_BUTTON_LEFT) {
			if (execution_mode) {

				// Distance calculation
				glm::vec3 player_pos = camera->transform->make_world_from_local()[3];
				glm::vec3 enemy_pos  = enemy->make_world_from_local()[3];

				float dist = glm::length(enemy_pos - player_pos);
				if (dist <= execution_range) {
					// === EXECUTION SUCCESS ===

				// enemy is dead
					enemy_alive = false;
					execution_mode = false;
					stalk_charge = 0.0f;

					// instantly make enemy disappear
					enemy->scale = glm::vec3(0.0f); // shrink to invisible
					enemy->position.z = -100.0f;    // move far below ground to ensure hidden
					kill_count += 1;
					if (kill_count >= 1) {
						dash_skill = true;
					} else if (kill_count >= 2) {
						dash_skill = true;
						attraction_ability = true;
					}

					if (deer_stage == 0)
					{
						// Level completes, gets leg // TODO: add some effect; and deer needs to attack enemy before geting their leg
						final_deer->scale = glm::vec3(0.0f); // hide original deer
						final_deer_leg->scale = glm::vec3(1.0f);
						deer_stage = 1;
					}

					return true;
				}
				else
				{
					// Not in distance
					return true;
				}
			}
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    if (evt.button.button == SDL_BUTTON_RIGHT) {
        // Toggle OFF:
        focus_mode = false;
		stalking = false;  
        player_speed_factor = 1.0f;
        target_fovy = base_fovy*2.1f; // restore zoom
        return true;
    	}
	}else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
			glm::vec2 motion = glm::vec2(evt.motion.xrel / float(window_size.y),
										 -evt.motion.yrel / float(window_size.y));

			cam->update_camera(motion * camera->fovy);
			return true;
		}
	}

	return false;
}

bool check_collision(glm::vec3 posA, glm::vec3 halfSizeA, glm::vec3 posB, glm::vec3 halfSizeB)
{
	return (std::abs(posA.x - posB.x) <= (halfSizeA.x + halfSizeB.x)) &&
		   (std::abs(posA.y - posB.y) <= (halfSizeA.y + halfSizeB.y)) &&
		   (std::abs(posA.z - posB.z) <= (halfSizeA.z + halfSizeB.z));
}

void PlayMode::update(float elapsed) {
	if (game_over) {
		// Optional: keep camera/UI effects, but block gameplay logic
		// camera->fovy = glm::mix(camera->fovy, target_fovy, 1.0f - std::exp(-elapsed * zoom_speed));
		return;
	}

	// --- Dash timers ---
	if (dash_cooldown_timer > 0.0f) {
		dash_cooldown_timer = std::max(0.0f, dash_cooldown_timer - elapsed);
	}
	if (dashing) {
		dash_timer -= elapsed;
		if (dash_timer <= 0.0f) {
			dashing = false;
			target_fovy = base_fovy; // restore zoom after dash
		}
	}

	// --- Enemy collapse animation (after execution) ---
	if (enemy && enemy_collapsing) {
		enemy_collapse_t += elapsed;
		float t = enemy_collapse_t / enemy_collapse_duration;
		if (t > 1.0f) t = 1.0f;

		// simple slerp from standing to “lying” pose
		enemy->rotation = glm::slerp(enemy_collapse_start, enemy_collapse_end, t);

		if (t >= 1.0f) {
			enemy_collapsing = false; // finished collapsing
		}
	}
	// animate human
	enemy->scale = glm::vec3(1.5f);
	// enemy_graph.update(elapsed);
	enemy_rig->update(elapsed);

	camera->fovy = glm::mix(camera->fovy, target_fovy, 1.0f - std::exp(-elapsed * zoom_speed));

	// --- Player movement (WASD, relative to camera) ---
	{
		if (dashing) {
        player->position += dash_dir * dash_speed * elapsed;
    } else {
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x = -1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y = -1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * player_speed_factor * elapsed;

		glm::mat4x3 frame = player->make_parent_from_local();
		glm::vec3 frame_right = -frame[0];
		glm::vec3 frame_forward = -frame[1];

		// player->position += move.x * frame_right + move.y * frame_forward;

		glm::vec3 new_pos = player->position + move.x * frame_right + move.y * frame_forward;

		// Define approximate hitboxes (adjust to your model scale)
		glm::vec3 player_half(1.0f, 1.0f, 1.0f);
		glm::vec3 gate_half(2.0f, 9.0f, 3.0f);
		glm::vec3 fence_half(2.0f, 6.0f, 3.0f);

		// Check collisions
		bool hit_gate = check_collision(new_pos, player_half, gate->position, gate_half);
		bool hit_fence = false;
		for (auto *f : fences)
		{
			if (!f)
				continue;
			if (check_collision(new_pos, player_half, f->position, fence_half))
			{
				hit_fence = true;
				break;
			}
		}

		if (!hit_gate && !hit_fence) player->position = new_pos; // Only move if no collision
		}
	}

	// --- Enemy on-screen check (clip-space) ---
	enemy_on_screen = false;
	if (enemy && enemy_alive) {
		glm::mat4 clip_from_world = camera->make_projection()
			* glm::mat4(camera->transform->make_local_from_world());
		glm::vec3 e_world = enemy->make_world_from_local()[3];
		glm::vec4 clip = clip_from_world * glm::vec4(e_world, 1.0f);
		if (clip.w > 0.0f) {
			glm::vec3 ndc = glm::vec3(clip) / clip.w; // [-1,1]
			enemy_on_screen = (ndc.x >= -1.0f && ndc.x <= 1.0f &&
							ndc.y >= -1.0f && ndc.y <= 1.0f);
		}
	}

	// --- Stalk bar charge/decay (depends on enemy on-screen visibility) ---
	if (stalking && enemy_on_screen && enemy_visible) {
		stalk_charge += stalk_charge_rate * elapsed;
		if (stalk_charge > 1.0f) {
			stalk_charge = 1.0f;
            execution_mode = true;
		}
	} 

	// --- Enemy sensing: FOV + distance (+ optional LOS hook) ---
	being_watched = false;
	if (enemy && player) {
		glm::mat4x3 e_world = enemy->make_world_from_local();
		glm::vec3 e_pos = e_world[3];
		glm::vec3 e_forward = -glm::vec3(e_world[1]); // -Y is "forward"

		glm::vec3 to_player3 = player->position - e_pos;
		float dist = glm::length(to_player3);

		if (dist > 0.0001f && dist <= enemy_view_distance) {
			glm::vec3 dir = to_player3 / dist;
			float cos_half_fov = std::cos(glm::radians(enemy_fov_deg * 0.5f));
			float cos_theta = glm::dot(glm::normalize(e_forward), dir);

			bool in_fov = (cos_theta > cos_half_fov) && (glm::dot(e_forward, to_player3) > 0.0f);

			// LOS hook (currently always unblocked):
			auto occluded_enemy_to_player = [&]() -> bool {
				// TODO: implement a real ray/occlusion test if desired
				return false;
			};
			bool blocked = occluded_enemy_to_player();

			if (in_fov && !blocked)
				being_watched = true;
		}
	}

	// --- Latch logic (sticky "seeing" state with grace timeout) ---
	if (being_watched) {
		watched_latched = true;
		watched_grace_timer = watched_grace;
		watched_accum += elapsed;
		if (watched_accum >= watch_to_gameover) {
			trigger_game_over();
		} else {
			// continuous requirement: reset if not watched this frame
			watched_accum = 0.0f;
		}
	} else if (watched_latched) {
		bool out_of_range = true;
		bool blocked_now = false;

		if (enemy && player) {
			glm::mat4x3 e_world = enemy->make_world_from_local();
			glm::vec3 e_pos = e_world[3];
			float dist = glm::length(player->position - e_pos);
			out_of_range = !(dist <= enemy_view_distance);

			auto occluded_enemy_to_player = [&]() -> bool { return false; };
			blocked_now = occluded_enemy_to_player();
		}

		if (out_of_range || blocked_now) {
			watched_grace_timer -= elapsed;
			if (watched_grace_timer <= 0.0f)
				watched_latched = false;
		} else {
			// still good; refresh
			watched_grace_timer = watched_grace;
		}
	}
	
	// --- Enemy behavior: Stand-and-watch vs Patrol ---
	
	if (enemy && enemy_alive && !enemy_collapsing) {
		// Compute planar vector to player for turning:
		glm::vec2 to_player_xy(0.0f);
		float to_player_dist = 0.0f;
		if (player) {
			glm::vec3 v = player->position - enemy->position;
			to_player_xy = glm::vec2(v.x, v.y);
			to_player_dist = glm::length(to_player_xy);
		}

		if (watched_latched) {
			// STAND STILL, ONLY ROTATE to face player while latched
			if (to_player_dist > 1e-4f) {
				glm::vec2 dir = to_player_xy / to_player_dist;
				float yaw = std::atan2(dir.x, dir.y); // +Y forward
				glm::quat target_rot =
					glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f)) *
					enemy_base_rotation;

				float turn_speed = 8.0f; // tweak feel
				enemy->rotation = glm::slerp(enemy->rotation, target_rot, 1.0f - std::exp(-turn_speed * elapsed));
			}
			// NO translation here -> feet frozen

		} else if (!enemy_waypoints.empty()) {
			// PATROL: waypoint walking (original logic)
			if (enemy_wait_timer > 0.0f) {
				enemy_wait_timer = std::max(0.0f, enemy_wait_timer - elapsed);
			} else {
				glm::vec3 target = enemy_waypoints[enemy_wp_idx];
				glm::vec2 to = glm::vec2(target.x - enemy->position.x,
										 target.y - enemy->position.y);
				float dist = glm::length(to);

				if (dist <= enemy_reach_epsilon) {
					enemy_wp_idx = (enemy_wp_idx + 1) % enemy_waypoints.size();
					enemy_wait_timer = enemy_wait_at_point;
				} else if (dist > 0.0f) {
					glm::vec2 dir = to / dist;
					float step = enemy_speed * elapsed;
					if (step > dist)
						step = dist;

					enemy->position.x += dir.x * step;
					enemy->position.y += dir.y * step;

					float yaw = std::atan2(dir.x, dir.y);
					glm::quat target_rot =
						glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f)) *
						enemy_base_rotation;
					enemy->rotation = glm::slerp(enemy->rotation, target_rot, 1.0f - std::exp(-elapsed * 8.0f));
				}
			}
		}
	}

	// update civilians
	for (auto &civilian : civilians) {
		civilian_update(civilian, elapsed);
	}
	resolve_collisions(civilians);
	civilian_avoid_obstacles(civilians, {{player, 0.7f}, {enemy, 0.7f}});

	// --- Audio listener follow player ---
	{
		glm::mat4x3 frame = player->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	// --- reset one-frame key counts ---
	left.downs = right.downs = up.downs = down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	// update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);
	glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());
	glm::vec3 eye = camera->transform->make_world_from_local()[3];

	// //set up light type and position for lit_color_texture_program:
	// // TODO: consider using the Light(s) in the scene to do this
	// glUseProgram(lit_color_texture_program->program);
	// glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	// glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	// glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	// glUseProgram(0);

	//--- draw geometry to framebuffer ---
	fb.resize(drawable_size);

	glBindFramebuffer(GL_FRAMEBUFFER, fb.objects_fb); // bind the objects (G-buffer) framebuffer as render target

	GLfloat zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // helper clear color (all channels = 0)
	glClearBufferfv(GL_COLOR, 0, zeros); // clear color attachment 0 (position texture) to zeros
	glClearBufferfv(GL_COLOR, 1, zeros); // clear color attachment 1 (normal + roughness) to zeros
	glClearBufferfv(GL_COLOR, 2, zeros); // clear color attachment 2 (albedo) to zeros
	glClear(GL_DEPTH_BUFFER_BIT); // clear the shared depth buffer

	glDisable(GL_BLEND); // disable blending for the geometry pass
	glEnable(GL_DEPTH_TEST); // enable depth testing so only nearest fragments write
	glDepthFunc(GL_LEQUAL); // depth test: pass if incoming depth <= stored depth

	//draw objects to geometry framebuffers:
	scene.draw(world_to_clip); // render scene into G-buffers using world->clip matrix

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // unbind framebuffer (return to default framebuffer)

	GL_ERRORS(); // check for GL errors (helper macro)

	//--- draw lights, reading geometry from framebuffer ---

	glBindFramebuffer(GL_FRAMEBUFFER, fb.lights_fb); // bind lights framebuffer (output accumulation texture)

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // set clear color for lights pass (transparent black)
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear both color and depth on lights framebuffer
	glDisable(GL_DEPTH_TEST); // disable standard depth testing for light accumulation
	glDepthFunc(GL_GREATER); // use reversed-depth test: pass if fragment depth > stored depth (used with front-face culling)

	glCullFace(GL_FRONT); // cull front faces so we render back faces of light volumes
	glEnable(GL_CULL_FACE); // enable face culling

	glEnable(GL_BLEND); // enable blending to accumulate light contributions
	glBlendEquation(GL_FUNC_ADD); // blending operation: add src and dst
	glBlendFunc(GL_ONE, GL_ONE); // additive blending: src*1 + dst*1
	glDepthMask(GL_FALSE); // disable depth writes while accumulating lighting (keep depth buffer from changing)

	//draw geometry for each light:
	auto &prog = basic_material_deferred_light_program; // reference to the deferred-light shader wrapper
	glUseProgram(prog->program); // bind the deferred-light shader program

	glBindVertexArray(light_for_basic_material_deferred_light); // bind VAO containing the light-volume geometry

	glActiveTexture(GL_TEXTURE0); // select texture unit 0
	glBindTexture(GL_TEXTURE_2D, fb.position_tex); // bind world-space position G-buffer to unit 0
	glActiveTexture(GL_TEXTURE1); // select texture unit 1
	glBindTexture(GL_TEXTURE_2D, fb.normal_roughness_tex); // bind normal+roughness G-buffer to unit 1
	glActiveTexture(GL_TEXTURE2); // select texture unit 2
	glBindTexture(GL_TEXTURE_2D, fb.albedo_tex); // bind albedo (color) G-buffer to unit 2

	for (auto const &light : scene.lights) { // iterate over all lights in the scene
		glm::mat4 light_to_world = light.transform->make_world_from_local(); // compute light's model-to-world transform

		Mesh const *mesh = nullptr; // pointer to chosen light-volume mesh for this light

		glUniform3fv(prog->EYE_vec3, 1, glm::value_ptr(eye)); // upload camera/eye position (in light-space)
		glUniform3fv(prog->LIGHT_LOCATION_vec3, 1, glm::value_ptr(glm::vec3(light_to_world[3]))); // upload light position
		glUniform3fv(prog->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(-light_to_world[2]))); // upload light direction (negated forward)
		glUniform3fv(prog->LIGHT_ENERGY_vec3, 1, glm::value_ptr(light.energy)); // upload light energy/color
		if (light.type == Scene::Light::Point) {
			glUniform1i(prog->LIGHT_TYPE_int, 0); // tell shader this is a point light
			glUniform1f(prog->LIGHT_CUTOFF_float, 1.0f); // cutoff not used for point here (set to 1)
			mesh = &light_meshes->cube; // use cube mesh as bounding volume for point light
			//when is energy / dis^2 < 1/256.0f?
			float R = std::sqrt(256.0f * std::max(light.energy.x, std::max(light.energy.y, light.energy.z))); // compute influence radius from energy
			light_to_world = light_to_world * glm::mat4( // scale the light-volume by R
				R, 0.0f, 0.0f, 0.0f,
				0.0f, R, 0.0f, 0.0f,
				0.0f, 0.0f, R, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		} else if (light.type == Scene::Light::Hemisphere) {
			glUniform1i(prog->LIGHT_TYPE_int, 1); // hemisphere light type
			glUniform1f(prog->LIGHT_CUTOFF_float, 1.0f); // no cutoff
			mesh = &light_meshes->everything; // full-screen geometry
			float R = 1.0f; // radius unused/1
			light_to_world = light_to_world * glm::mat4( // apply uniform scale of 1
				R, 0.0f, 0.0f, 0.0f,
				0.0f, R, 0.0f, 0.0f,
				0.0f, 0.0f, R, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		} else if (light.type == Scene::Light::Spot) {
			glUniform1i(prog->LIGHT_TYPE_int, 2); // spot light type
			glUniform1f(prog->LIGHT_CUTOFF_float, std::cos(0.5f * light.spot_fov)); // upload spot cutoff cosine
			mesh = &light_meshes->cone; // use cone mesh for spot volume
			float R = std::sqrt(256.0f * std::max(light.energy.x, std::max(light.energy.y, light.energy.z))); // estimate radius from energy
			//HACK: hard-limit to 5 units:
			R = 5.0f; // clamp radius to 5 to avoid huge cones
			float C = std::tan(0.5f * light.spot_fov); // cone radius factor from FOV
			light_to_world = light_to_world * glm::mat4( // scale cone by C*R (x/y) and R (z)
				C*R, 0.0f, 0.0f, 0.0f,
				0.0f, C*R, 0.0f, 0.0f,
				0.0f, 0.0f, R, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		} else if (light.type == Scene::Light::Directional) {
			glUniform1i(prog->LIGHT_TYPE_int, 3); // directional light type
			glUniform1f(prog->LIGHT_CUTOFF_float, 1.0f); // no cutoff
			mesh = &light_meshes->everything; // full-screen geometry for directional
			float R = 1.0f; // unused scale
			light_to_world = light_to_world * glm::mat4( // apply uniform scale of 1
				R, 0.0f, 0.0f, 0.0f,
				0.0f, R, 0.0f, 0.0f,
				0.0f, 0.0f, R, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		}
		glUniformMatrix4fv(prog->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(world_to_clip * light_to_world)); // upload light-volume transform to clip space

		if (mesh && mesh->count) { // if we have geometry, draw the light volume
			glDrawArrays(mesh->type, mesh->start, mesh->count); // render light-volume; shader reads G-buffers to accumulate lighting
		}
	}

	glActiveTexture(GL_TEXTURE2); // restore texture unit 2
	glBindTexture(GL_TEXTURE_2D, 0); // unbind texture from unit 2
	glActiveTexture(GL_TEXTURE1); // restore texture unit 1
	glBindTexture(GL_TEXTURE_2D, 0); // unbind texture from unit 1
	glActiveTexture(GL_TEXTURE0); // restore texture unit 0
	glBindTexture(GL_TEXTURE_2D, 0); // unbind texture from unit 0

	glBindVertexArray(0); // unbind VAO

	glDepthMask(GL_TRUE); // re-enable depth writes

	glDisable(GL_CULL_FACE); // disable face culling

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // unbind any framebuffer (back to default)

	GL_ERRORS(); // debug check for GL errors

	//--- copy lights fb info to screen ---
	
	glClearColor(0.2f, 0.2f, 0.2f, 0.0f);  // Set background to dark gray
	glClear(GL_COLOR_BUFFER_BIT);           // Clear the color buffer with the background color

	GL_ERRORS();
	glDisable(GL_BLEND);                    // Disable blending as we're doing a straight copy
	glDisable(GL_DEPTH_TEST);               // Disable depth testing as we're drawing a full-screen quad
	GL_ERRORS();

	glBindVertexArray(empty_vao);           // Bind the empty VAO for full-screen quad rendering
	glUseProgram(copy_to_screen_program->program);  // Use the program that copies textures to screen

	glActiveTexture(GL_TEXTURE0);           // Activate the first texture unit
	glBindTexture(GL_TEXTURE_2D, fb.output_tex);  // Show final lighting result

	// if (show == ShowOutput) {
	// 	glBindTexture(GL_TEXTURE_2D, fb.output_tex);  // Show final lighting result
	// } else if (show == ShowPosition) {
	// 	glBindTexture(GL_TEXTURE_2D, fb.position_tex);  // Show world-space positions
	// } else if (show == ShowNormalRoughness) {
	// 	glBindTexture(GL_TEXTURE_2D, fb.normal_roughness_tex);  // Show normals and roughness
	// } else if (show == ShowAlbedo) {
	// 	glBindTexture(GL_TEXTURE_2D, fb.albedo_tex);  // Show surface colors and textures
	// }

	GL_ERRORS();
	glDrawArrays(GL_TRIANGLES, 0, 3);       // Draw full-screen triangle (efficient full-screen quad)
	GL_ERRORS();

	glActiveTexture(GL_TEXTURE0);           // Reset active texture unit
	glBindTexture(GL_TEXTURE_2D, 0);        // Unbind texture

	glBindVertexArray(0);                   // Unbind VAO
	glUseProgram(0);                        // Unbind shader program

	//--- stalking mechanics ---
	if (focus_mode) {
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // stark white background for high contrast
	} else if (execution_mode) {
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f); // red background during execution mode
	} else {
		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	}
	glClearDepth(1.0f); // 1.0 is actually the default value to clear the depth
						// buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// We already have the lit color on the default framebuffer.
	// Now we only want depth, so disable color writes:
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.
	glDepthMask(GL_TRUE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthFunc(GL_LESS); // this is the default depth comparison function, but
						  // FYI you can change it.

	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_DEPTH_BUFFER_BIT); // clears depth only (color is masked off)

	// Draw the scene with your normal pipelines; this will fill depth,
	// but leave the deferred-lit color untouched:
	scene.draw(*camera);

	// Re-enable color writes for later overlays:
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	enemy_visible = false; // default

	if (enemy && enemy_alive) {
		glm::mat4 clip_from_world = camera->make_projection()
			* glm::mat4(camera->transform->make_local_from_world());
		glm::vec3 e_world = enemy->make_world_from_local()[3];
		glm::vec4 clip = clip_from_world * glm::vec4(e_world, 1.0f);

		if (clip.w > 0.0f) {
			glm::vec3 ndc = glm::vec3(clip) / clip.w; // [-1,1]
			// Outside of screen
			if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) {
				enemy_visible = false;
			} else {
				float sx = (ndc.x * 0.5f + 0.5f) * drawable_size.x;
				float sy = (ndc.y * 0.5f + 0.5f) * drawable_size.y;
				int px = std::clamp(int(std::lround(sx)), 0, int(drawable_size.x) - 1);
				int py = std::clamp(int(std::lround(sy)), 0, int(drawable_size.y) - 1);

				float depth_sample = 1.0f;
				glReadPixels(px, py, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth_sample);

				float enemy_depth = ndc.z * 0.5f + 0.5f;
				const float eps = 1e-3f;

				enemy_visible = !(depth_sample + eps < enemy_depth);
			}
		}
	}
	if (focus_mode && enemy && enemy_visible) {
		// project enemy world position to clip space:
		glm::mat4 clip_from_world =
			camera->make_projection() *
			glm::mat4(camera->transform->make_local_from_world());

		glm::mat4x3 world_from_enemy = enemy->make_world_from_local();
		glm::vec3 e_world = world_from_enemy[3]; // translation column
		glm::vec4 e_clip = clip_from_world * glm::vec4(e_world, 1.0f);

		if (e_clip.w > 0.0f) {
			glm::vec3 e_ndc = glm::vec3(e_clip) / e_clip.w; // [-1,1] range
			// set up 2D line drawer (same as your text HUD uses)
			glDisable(GL_DEPTH_TEST);
			float aspect = float(drawable_size.x) / float(drawable_size.y);
			DrawLines lines(glm::mat4(1.0f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));

			// Convert NDC to the DrawLines' coords: x in [-aspect, aspect], y in
			// [-1,1]
			glm::vec3 p(e_ndc.x * aspect, e_ndc.y, 0.0f);

			// crosshair size (in screen space units):
			const float s = 0.05f;

			// crosshair lines (black for visibility on white bg):
			glm::u8vec4 col(0x00, 0x00, 0x00, 0xff);
			lines.draw(p + glm::vec3(-s, 0.0f, 0.0f), p + glm::vec3(+s, 0.0f, 0.0f), col);
			lines.draw(p + glm::vec3(0.0f, -s, 0.0f), p + glm::vec3(0.0f, +s, 0.0f), col);

			// "ENEMY" label just above the crosshair:
			const float H = 0.06f;
			lines.draw_text("ENEMY", p + glm::vec3(-0.5f * H, +1.4f * H, 0.0f), glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f), col);
			glEnable(GL_DEPTH_TEST);
		}
	}
	if (focus_mode && enemy) {
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(1.0f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));

		// bar geometry (screen space): centered, near bottom
		const float bar_w = 1.6f;  // total width
		const float bar_h = 0.08f; // height
		const float y = -0.90f;	   // vertical position
		const float x0 = -0.5f * bar_w;
		const float x1 = 0.5f * bar_w;
		const float y0 = y;
		const float y1 = y + bar_h;

		// outline (light gray)
		glm::u8vec4 outline(0xcc, 0xcc, 0xcc, 0xff);
		lines.draw(glm::vec3(x0, y0, 0.0f), glm::vec3(x1, y0, 0.0f), outline);
		lines.draw(glm::vec3(x1, y0, 0.0f), glm::vec3(x1, y1, 0.0f), outline);
		lines.draw(glm::vec3(x1, y1, 0.0f), glm::vec3(x0, y1, 0.0f), outline);
		lines.draw(glm::vec3(x0, y1, 0.0f), glm::vec3(x0, y0, 0.0f), outline);

		// background (empty) – thin gray center line just for context (optional)
		glm::u8vec4 back(0x55, 0x55, 0x55, 0xff);
		lines.draw(glm::vec3(x0, (y0 + y1) * 0.5f, 0.0f),
				   glm::vec3(x1, (y0 + y1) * 0.5f, 0.0f),
				   back);

		// FILLED BLACK RECTANGLE that grows with stalk_charge:
		const float fill_x = x0 + (x1 - x0) * stalk_charge;
		glm::u8vec4 black(0x00, 0x00, 0x00, 0xff);

		// scan-fill using horizontal lines
		const int stripes = 48; // more = more solid-looking fill
		for (int i = 0; i < stripes; ++i) {
			float t0 = float(i) / stripes;
			float y_line = y0 + t0 * bar_h;
			lines.draw(glm::vec3(x0, y_line, 0.0f), glm::vec3(fill_x, y_line, 0.0f), black);
		}

		// label
		const float H = 0.06f;
		lines.draw_text("STALK", glm::vec3(x0, y1 + 0.02f, 0.0f), glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f), outline);

		glEnable(GL_DEPTH_TEST);
	}
	if (being_watched) {
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(1.0f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
		// Centered-ish: start slightly left of (0,0)
		constexpr float H = 0.14f; // text size
		glm::u8vec4 warn = glm::u8vec4(0xff, 0x40, 0x40, 0xff);
		lines.draw_text(
			"You are being watched!",
			glm::vec3(-0.55f, 0.02f, 0.0f), // tweak to taste for centering
			glm::vec3(H, 0.0f, 0.0f),		// x step
			glm::vec3(0.0f, H, 0.0f),		// y step
			warn);
		glEnable(GL_DEPTH_TEST);
	}
	
	if (game_over) {
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(1.0f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
		constexpr float H = 0.18f;								 // slightly larger text
		glm::u8vec4 color = glm::u8vec4(0xff, 0x00, 0x00, 0xff); // bright red
		lines.draw_text("Zoo has been locked",
						glm::vec3(-0.7f, 0.0f, 0.0f), // centered-ish position
						glm::vec3(H, 0.0f, 0.0f),
						glm::vec3(0.0f, H, 0.0f),
						color);
		glEnable(GL_DEPTH_TEST);
	}
	
	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(1.0f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));

		constexpr float H = 0.05f;
		lines.draw_text("WASD moves character. Right click to stalk the human visitor to learn how human walks. Left click to attack when you have finished learning...",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("WASD moves character. Right click to stalk the human visitor to learn how human walks. Left click to attack when you have finished learning...",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	/*
	if (deer_ui_tex && deer_ui_size.x && deer_ui_size.y) {
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		float wpx = deer_ui_target_width_px;
		float hpx = wpx * float(deer_ui_size.y) / float(deer_ui_size.x);
		const float pad = 8.0f;

		auto px2ndc = [&](float x_px, float y_px)->glm::vec2 {
			return {
				(x_px / float(drawable_size.x)) * 2.0f - 1.0f,
				(y_px / float(drawable_size.y)) * 2.0f - 1.0f
			};
		};
		glm::vec2 p00 = px2ndc(pad,        pad       );
		glm::vec2 p10 = px2ndc(pad + wpx,  pad       );
		glm::vec2 p11 = px2ndc(pad + wpx,  pad + hpx );
		glm::vec2 p01 = px2ndc(pad,        pad + hpx );

		float vtx[6*4] = {
			p00.x,p00.y, 0,0,  p10.x,p10.y, 1,0,  p11.x,p11.y, 1,1,
			p00.x,p00.y, 0,0,  p11.x,p11.y, 1,1,  p01.x,p01.y, 0,1
		};

		glBindBuffer(GL_ARRAY_BUFFER, deer_ui_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vtx), vtx);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glUseProgram(lit_color_texture_program->program);

		GLint u_obj = glGetUniformLocation(lit_color_texture_program->program, "OBJECT_TO_CLIP");
		if (u_obj < 0) u_obj = glGetUniformLocation(lit_color_texture_program->program, "OBJECT_TO_CLIP_mat4");
		if (u_obj >= 0) {
			glm::mat4 I(1.0f);
			glUniformMatrix4fv(u_obj, 1, GL_FALSE, glm::value_ptr(I));
		}

		GLint u_samp = glGetUniformLocation(lit_color_texture_program->program, "COLOR_TEXTURE");
		if (u_samp >= 0) glUniform1i(u_samp, 0);
		GLint u_use = glGetUniformLocation(lit_color_texture_program->program, "UseTexture");
		if (u_use >= 0) glUniform1i(u_use, 1);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, deer_ui_tex);

		glBindVertexArray(deer_ui_vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);

		glDisable(GL_BLEND);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
	}*/
	GL_ERRORS();
}
