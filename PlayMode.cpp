#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint zoo_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > zoo_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("zoo_nolink.pnct"));
	zoo_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > zoo_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("zoo_nolink.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = zoo_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = zoo_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});


PlayMode::PlayMode() : scene(*zoo_scene) {
	//get pointers to transforms for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Player") player = &transform;
		if (transform.name == "Enemy") enemy = &transform;
		if (transform.name == "Final_Deer") final_deer = &transform;
		if (transform.name == "Final_Deer Leg") {
			final_deer_leg = &transform;
			transform.scale = glm::vec3(0.0f); // set invisible initially
		}
	}
	if (player == nullptr) throw std::runtime_error("Player not found.");
	if (enemy == nullptr) throw std::runtime_error("enemy not found.");
	if (final_deer == nullptr) throw std::runtime_error("final_deer not found.");
	if (final_deer_leg == nullptr) throw std::runtime_error("final_deer_leg not found.");

	player_base_rotation = player->rotation;

	//get pointer to camera for convenience:
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
		e0 + glm::vec3( 0.0f,  R, 0.0f),
		e0 + glm::vec3( R,  0.0f, 0.0f),
		e0 + glm::vec3( 0.0f, -R, 0.0f),
		e0 + glm::vec3(-R,  0.0f, 0.0f)
	};
	enemy_wp_idx = 0;
	enemy_wait_timer = 0.0f;
}

void PlayMode::trigger_game_over() {
	if (game_over) return;           // idempotent
	game_over = true;

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
			player_speed_factor = 0.2f;     // slow to 50%
			target_fovy = base_fovy * 0.7f; // zoom in a bit
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    if (evt.button.button == SDL_BUTTON_RIGHT) {
        // Toggle OFF:
        focus_mode = false;
		stalking = false;  
        player_speed_factor = 1.0f;
        target_fovy = base_fovy; // restore zoom
        return true;
    	}
	}else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);

			cam->update_camera(motion * camera->fovy);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	// --- Camera zoom tween ---
		if (game_over) {
		// Optional: keep camera/UI effects, but block gameplay logic
		// camera->fovy = glm::mix(camera->fovy, target_fovy, 1.0f - std::exp(-elapsed * zoom_speed));
		return;
	}
	camera->fovy = glm::mix(camera->fovy, target_fovy, 1.0f - std::exp(-elapsed * zoom_speed));

	// --- Player movement (WASD, relative to camera) ---
	{
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed  && !right.pressed) move.x = -1.0f;
		if (!left.pressed &&  right.pressed) move.x =  1.0f;
		if (down.pressed  && !up.pressed)    move.y = -1.0f;
		if (!down.pressed &&  up.pressed)    move.y =  1.0f;

		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * player_speed_factor * elapsed;

		glm::mat4x3 frame = player->make_parent_from_local();
		glm::vec3 frame_right   = -frame[0];
		glm::vec3 frame_forward = -frame[1];

		player->position += move.x * frame_right + move.y * frame_forward;
	}

	// --- Stalk bar charge/decay (depends on enemy on-screen visibility) ---
	if (stalking && enemy_visible) {
		stalk_charge += stalk_charge_rate * elapsed;
		if (stalk_charge > 1.0f) stalk_charge = 1.0f;
	} else {
		stalk_charge -= stalk_decay_rate * elapsed;
		if (stalk_charge < 0.0f) stalk_charge = 0.0f;
	}

	if (stalk_charge >= 1.0f) {
		if (deer_stage == 0) {
			// Level completes, gets leg // TODO: add some effect; and deer needs to attack enemy before geting their leg
			final_deer->scale = glm::vec3(0.0f); // hide original deer
			final_deer_leg->scale = glm::vec3(1.0f);
			deer_stage = 1;
		}
	}

	if (stalk_charge >= 1.0f) {
		if (deer_stage == 0) {
			// Level completes, gets leg // TODO: add some effect; and deer needs to attack enemy before geting their leg
			final_deer->scale = glm::vec3(0.0f); // hide original deer
			final_deer_leg->scale = glm::vec3(1.0f);
			deer_stage = 1;
		}
	}

	// --- Enemy sensing: FOV + distance (+ optional LOS hook) ---
	being_watched = false;
	if (enemy && player) {
		glm::mat4x3 e_world = enemy->make_world_from_local();
		glm::vec3 e_pos     = e_world[3];
		glm::vec3 e_forward = -glm::vec3(e_world[1]); // -Y is "forward"

		glm::vec3 to_player3 = player->position - e_pos;
		float dist = glm::length(to_player3);

		if (dist > 0.0001f && dist <= enemy_view_distance) {
			glm::vec3 dir = to_player3 / dist;
			float cos_half_fov = std::cos(glm::radians(enemy_fov_deg * 0.5f));
			float cos_theta    = glm::dot(glm::normalize(e_forward), dir);

			bool in_fov = (cos_theta > cos_half_fov) && (glm::dot(e_forward, to_player3) > 0.0f);

			// LOS hook (currently always unblocked):
			auto occluded_enemy_to_player = [&]()->bool {
				// TODO: implement a real ray/occlusion test if desired
				return false;
			};
			bool blocked = occluded_enemy_to_player();

			if (in_fov && !blocked) being_watched = true;
		}
	}

	// --- Latch logic (sticky "seeing" state with grace timeout) ---
	if (being_watched) {
		watched_latched = true;
		watched_grace_timer = watched_grace;
		watched_accum += elapsed;
		if (watched_accum >= watch_to_gameover) {
			trigger_game_over();
		}else {
			// continuous requirement: reset if not watched this frame
			watched_accum = 0.0f;
		}
	} else if (watched_latched) {
		bool out_of_range = true;
		bool blocked_now  = false;

		if (enemy && player) {
			glm::mat4x3 e_world = enemy->make_world_from_local();
			glm::vec3 e_pos     = e_world[3];
			float dist = glm::length(player->position - e_pos);
			out_of_range = !(dist <= enemy_view_distance);

			auto occluded_enemy_to_player = [&]()->bool { return false; };
			blocked_now = occluded_enemy_to_player();
		}

		if (out_of_range || blocked_now) {
			watched_grace_timer -= elapsed;
			if (watched_grace_timer <= 0.0f) watched_latched = false;
		} else {
			// still good; refresh
			watched_grace_timer = watched_grace;
		}
	}

	// --- Enemy behavior: Stand-and-watch vs Patrol ---
	if (enemy) {
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
					glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f)) * enemy_base_rotation;

				float turn_speed = 8.0f; // tweak feel
				enemy->rotation = glm::slerp(enemy->rotation, target_rot,
				                             1.0f - std::exp(-turn_speed * elapsed));
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
					if (step > dist) step = dist;

					enemy->position.x += dir.x * step;
					enemy->position.y += dir.y * step;

					float yaw = std::atan2(dir.x, dir.y);
					glm::quat target_rot =
						glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f)) * enemy_base_rotation;
					enemy->rotation = glm::slerp(enemy->rotation, target_rot,
					                             1.0f - std::exp(-elapsed * 8.0f));
				}
			}
		}
	}

	// --- Audio listener follow player ---
	{
		glm::mat4x3 frame = player->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at    = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	// --- reset one-frame key counts ---
	left.downs = right.downs = up.downs = down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	if (focus_mode) {
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // stark white background for high contrast
	} else {
		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	}
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);
	enemy_visible = true; // default

	
	if (enemy) {
		// Project enemy position to screen:
		glm::mat4 clip_from_world = camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());
		glm::mat4x3 world_from_enemy = enemy->make_world_from_local();
		glm::vec3 e_world = world_from_enemy[3];

		glm::vec4 clip = clip_from_world * glm::vec4(e_world, 1.0f);
		if (clip.w > 0.0f) {
			glm::vec3 ndc = glm::vec3(clip) / clip.w; // [-1,1]
			// Window coords (pixels):
			float sx = (ndc.x * 0.5f + 0.5f) * drawable_size.x;
			float sy = (ndc.y * 0.5f + 0.5f) * drawable_size.y;
			float enemy_depth = ndc.z * 0.5f + 0.5f; // [0,1]

			// Sample a small box around the enemy (e.g., ~20px radius):
			const int radius = 20;
			const int step   = 10;  // stride between samples
			const float eps  = 1e-3f;

			int total = 0;
			int occluded = 0;

			for (int dy = -radius; dy <= radius; dy += step) {
				for (int dx = -radius; dx <= radius; dx += step) {
					int px = int(sx) + dx;
					int py = int(sy) + dy;
					if (px < 0 || py < 0 || px >= int(drawable_size.x) || py >= int(drawable_size.y)) continue;

					float depth_sample = 1.0f;
					glReadPixels(px, py, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth_sample);

					// If the sample depth is *in front of* the enemy (smaller), that point is blocked:
					if (depth_sample + eps < enemy_depth) occluded++;
					total++;
				}
			}
			// "Fully blocked" ≈ all tested points occluded:
			if (total > 0 && occluded == total) enemy_visible = false;
		} else {
			// behind camera
			enemy_visible = false;
		}
	}
	if (focus_mode && enemy && enemy_visible) {
		// project enemy world position to clip space:
		glm::mat4 clip_from_world = camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());

		glm::mat4x3 world_from_enemy = enemy->make_world_from_local();
		glm::vec3 e_world = world_from_enemy[3];           // translation column
		glm::vec4 e_clip  = clip_from_world * glm::vec4(e_world, 1.0f);

		if (e_clip.w > 0.0f) {
			glm::vec3 e_ndc = glm::vec3(e_clip) / e_clip.w; // [-1,1] range
			// set up 2D line drawer (same as your text HUD uses)
			glDisable(GL_DEPTH_TEST);
			float aspect = float(drawable_size.x) / float(drawable_size.y);
			DrawLines lines(glm::mat4(
				1.0f / aspect, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			));

			// Convert NDC to the DrawLines' coords: x in [-aspect, aspect], y in [-1,1]
			glm::vec3 p(e_ndc.x * aspect, e_ndc.y, 0.0f);

			// crosshair size (in screen space units):
			const float s = 0.05f;

			// crosshair lines (black for visibility on white bg):
			glm::u8vec4 col(0x00, 0x00, 0x00, 0xff);
			lines.draw(p + glm::vec3(-s, 0.0f, 0.0f), p + glm::vec3(+s, 0.0f, 0.0f), col);
			lines.draw(p + glm::vec3(0.0f, -s, 0.0f), p + glm::vec3(0.0f, +s, 0.0f), col);

			// "ENEMY" label just above the crosshair:
			const float H = 0.06f;
			lines.draw_text("ENEMY",
				p + glm::vec3(-0.5f * H, +1.4f * H, 0.0f),
				glm::vec3(H, 0.0f, 0.0f),
				glm::vec3(0.0f, H, 0.0f),
				col
			);
			glEnable(GL_DEPTH_TEST);
		}
	}
	if (focus_mode && enemy) {
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		// bar geometry (screen space): centered, near bottom
		const float bar_w = 1.6f;     // total width
		const float bar_h = 0.08f;    // height
		const float y     = -0.90f;   // vertical position
		const float x0    = -0.5f * bar_w;
		const float x1    =  0.5f * bar_w;
		const float y0    = y;
		const float y1    = y + bar_h;

		// outline (light gray)
		glm::u8vec4 outline(0xcc, 0xcc, 0xcc, 0xff);
		lines.draw(glm::vec3(x0, y0, 0.0f), glm::vec3(x1, y0, 0.0f), outline);
		lines.draw(glm::vec3(x1, y0, 0.0f), glm::vec3(x1, y1, 0.0f), outline);
		lines.draw(glm::vec3(x1, y1, 0.0f), glm::vec3(x0, y1, 0.0f), outline);
		lines.draw(glm::vec3(x0, y1, 0.0f), glm::vec3(x0, y0, 0.0f), outline);

		// background (empty) – thin gray center line just for context (optional)
		glm::u8vec4 back(0x55, 0x55, 0x55, 0xff);
		lines.draw(glm::vec3(x0, (y0+y1)*0.5f, 0.0f), glm::vec3(x1, (y0+y1)*0.5f, 0.0f), back);

		// FILLED BLACK RECTANGLE that grows with stalk_charge:
		const float fill_x = x0 + (x1 - x0) * stalk_charge;
		glm::u8vec4 black(0x00, 0x00, 0x00, 0xff);

		// scan-fill using horizontal lines
		const int stripes = 48; // more = more solid-looking fill
		for (int i = 0; i < stripes; ++i) {
			float t0 = float(i) / stripes;
			float y_line = y0 + t0 * bar_h;
			lines.draw(glm::vec3(x0,    y_line, 0.0f),
					glm::vec3(fill_x, y_line, 0.0f),
					black);
		}

		// label
		const float H = 0.06f;
		lines.draw_text("STALK",
			glm::vec3(x0, y1 + 0.02f, 0.0f),
			glm::vec3(H, 0.0f, 0.0f),
			glm::vec3(0.0f, H, 0.0f),
			outline
		);

		glEnable(GL_DEPTH_TEST);
	}
	
	if (being_watched) {
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));
		// Centered-ish: start slightly left of (0,0)
		constexpr float H = 0.14f; // text size
		glm::u8vec4 warn = glm::u8vec4(0xff, 0x40, 0x40, 0xff);
		lines.draw_text("You are being watched!",
			glm::vec3(-0.55f, 0.02f, 0.0f),   // tweak to taste for centering
			glm::vec3(H, 0.0f, 0.0f),         // x step
			glm::vec3(0.0f, H, 0.0f),         // y step
			warn
		);
		glEnable(GL_DEPTH_TEST);
	}
	
	if (game_over) {
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));
		constexpr float H = 0.18f; // slightly larger text
		glm::u8vec4 color = glm::u8vec4(0xff, 0x00, 0x00, 0xff); // bright red
		lines.draw_text("Zoo has been locked",
			glm::vec3(-0.7f, 0.0f, 0.0f),  // centered-ish position
			glm::vec3(H, 0.0f, 0.0f),
			glm::vec3(0.0f, H, 0.0f),
			color
		);
		glEnable(GL_DEPTH_TEST);
	}
	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("WASD moves character. Right click to stalk the human visitor to learn how human walks...",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("WASD moves character. Right click to stalk the human visitor to learn how human walks...",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}
