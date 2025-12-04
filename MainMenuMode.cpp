#include "MainMenuMode.hpp"

// next mode
#include "PlayMode.hpp"

#include "Load.hpp"
#include "data_path.hpp"

#include "Mesh.hpp"
#include "LightMeshes.hpp"
#include "Textures.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "gl_errors.hpp"

#include "BasicMaterialDeferredProgram.hpp"
#include "CopyToScreenProgram.hpp"
#include "check_fb.hpp"

GLuint menu_for_basic_material_deferred_object = 0;
GLuint menu_light_for_basic_material_deferred_light = 0;
Load< MeshBuffer > menu_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("zoo_nolink.pnct"));
	menu_for_basic_material_deferred_object = ret->make_vao_for_program(basic_material_deferred_object_program->program);
	return ret;
});

Load< Scene > menu_scene_deferred(LoadTagDefault, []() -> Scene const * {
	menu_light_for_basic_material_deferred_light = light_meshes->make_vao_for_program(basic_material_deferred_light_program->program);

	Scene *ret = new Scene(data_path("main_menu.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = menu_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		// drawable.pipeline = lit_color_texture_program_pipeline;
		drawable.pipeline = basic_material_deferred_object_program_pipeline;

		drawable.pipeline.vao = menu_for_basic_material_deferred_object;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

		float roughness = 1.0f;
		// if (transform->name.substr(0, 9) == "Icosphere") { //TODO: change name
		// 	roughness = (transform->position.y + 10.0f) / 18.0f;
		// }

		// printf("-- Transform name: %s\n", transform->name.c_str());
		for (size_t i = 0; i < named_textures.size(); ++i)
		{
			// printf("Checking prefix: %s\n", named_textures[i].prefix.c_str());
			if (transform->name.rfind(named_textures[i].prefix, 0) != std::string::npos)
			{
				if (i < textures->size())
				{
					// printf("Assigning texture %s to object %s\n", named_textures[i].filename.c_str(), transform->name.c_str());
					drawable.pipeline.textures[0].texture = (*textures)[i];
					drawable.pipeline.textures[0].target = GL_TEXTURE_2D;
				}
				break;
			}
		}

		bool is_sky = (transform->name == "Sky");
		drawable.pipeline.set_uniforms = [roughness, is_sky]()
		{
			glUniform1f(basic_material_deferred_object_program->ROUGHNESS_float, roughness);
			glUniform1i(basic_material_deferred_object_program->SKY_MODE_int,
						is_sky ? 1 : 0);
		};
	});

	return ret; 
});

// Helper: maintain a framebuffer to hold rendered geometry
struct FB {
	// object data gets stored in these textures:
	GLuint position_tex = 0;
	GLuint normal_roughness_tex = 0;
	GLuint albedo_tex = 0;
	
	//output image gets written to this texture:
	GLuint output_tex = 0;

	//depth buffer is shared between objects + lights pass:
	GLuint depth_rb = 0;

	GLuint objects_fb = 0; //(position, normal, albedo) + depth
	GLuint lights_fb = 0;  //(output) + depth

	glm::uvec2 size = glm::uvec2(0);

	void resize(glm::uvec2 const &drawable_size) {
		if (drawable_size == size) return;
		size = drawable_size;

		// helper to allocate a texture:
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

		// set up position_tex as a 32-bit floating point RGB texture:
		alloc_tex(position_tex, GL_RGB32F);

		// set up normal_roughness_tex as a 16-bit floating point RGBA texture:
		alloc_tex(normal_roughness_tex, GL_RGBA16F);

		// set up albedo_tex as an 8-bit fixed point RGBA texture:
		alloc_tex(albedo_tex, GL_RGBA8);

		// set up output_tex as an 8-bit fixed point RGBA texture:
		alloc_tex(output_tex, GL_RGBA8);

		// if depth_rb does not have a name, name it:
		if (depth_rb == 0) glGenRenderbuffers(1, &depth_rb);
		// set up depth_rb as a 24-bit fixed-point depth buffer:
		glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		// if objects framebuffer doesn't have a name, name it and attach textures:
		if (objects_fb == 0) {
			glGenFramebuffers(1, &objects_fb);
			// set up framebuffer: (don't need to do when resizing)
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

		// if lights-drawing framebuffer doesn't have a name, name it and attach textures:
		if (lights_fb == 0) {
			glGenFramebuffers(1, &lights_fb);
			// set up framebuffer: (don't need to do when resizing)
			glBindFramebuffer(GL_FRAMEBUFFER, lights_fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_tex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
			GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
			glDrawBuffers(1, bufs);
			check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}
} menu_fb;

MainMenuMode::MainMenuMode() : scene(*menu_scene_deferred) {
	overlay.add_element("Antlers", 
		glm::vec2(0.f, 200.f), 
		glm::vec2(1321.f, 334.f) * .625f,
		glm::vec4(.09f, .02f, 0.f, 1.f),
		ui_textures->find("Title Antlers")->second
	);
	overlay.add_element("Person", 
		glm::vec2(0.f, 200.f), 
		glm::vec2(1321.f, 334.f) * .625f,
		glm::vec4(.2f, 0.f, 0.f, 1.f),
		ui_textures->find("Title Person")->second
	);
	overlay.add_element("Title", 
		glm::vec2(0.f, 200.f), 
		glm::vec2(1321.f, 334.f) * .625f,
		glm::vec4(1.f),
		ui_textures->find("Title Words")->second
	);
    overlay.add_element("Play Button", 
		glm::vec2(0.f, -50.f), 
		glm::vec2(896.f, 384.f) * .25f, 
		glm::vec4(1.f), 
		ui_textures->find("Play Button")->second
	);
    overlay.add_interaction("Play Button", []() -> void { 
        Mode::set_current(std::make_shared< PlayMode >());
    });

	// get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

MainMenuMode::~MainMenuMode() {
}

bool MainMenuMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
    if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (evt.button.button == SDL_BUTTON_LEFT){
            float mouse_x = evt.button.x;
            float mouse_y = (window_size.y - evt.button.y);
			overlay.handle_click(glm::uvec2(mouse_x, mouse_y));
        }
    }
    return false;
}

void MainMenuMode::update(float elapsed) {
}

void MainMenuMode::draw(glm::uvec2 const &drawable_size) {
	//11/23 Update
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);
	glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());
	glm::vec3 eye = camera->transform->make_world_from_local()[3];

	//--- draw geometry to framebuffer ---
	menu_fb.resize(drawable_size);

	glBindFramebuffer(GL_FRAMEBUFFER, menu_fb.objects_fb); // bind the objects (G-buffer) framebuffer as render target

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

	glBindFramebuffer(GL_FRAMEBUFFER, menu_fb.lights_fb); // bind lights framebuffer (output accumulation texture)

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

	glBindVertexArray(menu_light_for_basic_material_deferred_light); // bind VAO containing the light-volume geometry

	glActiveTexture(GL_TEXTURE0); // select texture unit 0
	glBindTexture(GL_TEXTURE_2D, menu_fb.position_tex); // bind world-space position G-buffer to unit 0
	glActiveTexture(GL_TEXTURE1); // select texture unit 1
	glBindTexture(GL_TEXTURE_2D, menu_fb.normal_roughness_tex); // bind normal+roughness G-buffer to unit 1
	glActiveTexture(GL_TEXTURE2); // select texture unit 2
	glBindTexture(GL_TEXTURE_2D, menu_fb.albedo_tex); // bind albedo (color) G-buffer to unit 2

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
	glBindTexture(GL_TEXTURE_2D, menu_fb.output_tex);  // Show final lighting result

	// if (show == ShowOutput) {
	// 	glBindTexture(GL_TEXTURE_2D, menu_fb.output_tex);  // Show final lighting result
	// } else if (show == ShowPosition) {
	// 	glBindTexture(GL_TEXTURE_2D, menu_fb.position_tex);  // Show world-space positions
	// } else if (show == ShowNormalRoughness) {
	// 	glBindTexture(GL_TEXTURE_2D, menu_fb.normal_roughness_tex);  // Show normals and roughness
	// } else if (show == ShowAlbedo) {
	// 	glBindTexture(GL_TEXTURE_2D, menu_fb.albedo_tex);  // Show surface colors and textures
	// }

	GL_ERRORS();
	glDrawArrays(GL_TRIANGLES, 0, 3);       // Draw full-screen triangle (efficient full-screen quad)
	GL_ERRORS();

	glActiveTexture(GL_TEXTURE0);           // Reset active texture unit
	glBindTexture(GL_TEXTURE_2D, 0);        // Unbind texture

	glBindVertexArray(0);                   // Unbind VAO
	glUseProgram(0);                        // Unbind shader program

	glm::mat4 clip_from_world = camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());
	glm::mat4x3 light_from_world = glm::mat4x3(1.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    overlay.resize(drawable_size);
    overlay.draw();

    // copy overlay to screen

	glBindVertexArray(empty_vao);
	glUseProgram(copy_to_screen_program->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, overlay.tex);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
	GL_ERRORS();
}