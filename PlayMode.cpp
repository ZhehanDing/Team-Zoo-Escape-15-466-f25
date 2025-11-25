#include "PlayMode.hpp"
#include <algorithm>
#include <cmath>
// #include "LitColorTextureProgram.hpp"
#include "BasicMaterialDeferredProgram.hpp"
#include "LightMeshes.hpp"
#include "CopyToScreenProgram.hpp"
#include "ParticleProgram.hpp"
#include <memory>
#include "Textures.hpp"
#include "Collision.hpp"

#include "Animation.hpp"
#include "DrawLines.hpp"
#include "Load.hpp"
#include "Mesh.hpp"
#include "RiggedMesh.hpp"
#include "Skeleton.hpp"
#include "SkinningProgram.hpp"
#include "SkinningDeferredProgram.hpp"
#include "gl_errors.hpp"
#include "load_save_png.hpp"
#include "data_path.hpp"
#include "check_fb.hpp"

#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <random>
#include <map>
#include <functional>
#include <memory>

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
Load< Sound::Sample > bg_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("BackgroundMusic.wav"));
});
Load< Sound::Sample > gate_open_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("GateOpen.wav"));
});
Load< Sound::Sample > footstep_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("FootStep.wav"));
});
Load< Sound::Sample > stalking_completed_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("StalkingCompleted.wav"));
});
Load< Sound::Sample > stalking_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Stalking.wav"));
});
Load< Sound::Sample > start_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Start.wav"));
});
Load< Sound::Sample > attack_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Attack.wav"));
});
Load< Sound::Sample > enemy_die_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("EnemyDie.wav"));
});

Load< std::vector< Sound::Sample > > footstep_sounds(LoadTagDefault, []() -> std::vector< Sound::Sample > const * {
	std::vector< std::string > filenames = {
        "sounds/footsteps-01.wav", "sounds/footsteps-02.wav", "sounds/footsteps-03.wav",
		"sounds/footsteps-04.wav", "sounds/footsteps-05.wav", "sounds/footsteps-06.wav",
		"sounds/footsteps-07.wav", "sounds/footsteps-08.wav", "sounds/footsteps-09.wav",
		"sounds/footsteps-10.wav", "sounds/footsteps-11.wav",
    };
    auto ret = new std::vector< Sound::Sample >();
    ret->reserve(filenames.size());

    for (size_t i = 0; i < filenames.size(); ++i) {
        ret->emplace_back(Sound::Sample(data_path(filenames[i])));
    }

    return ret;
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
		// if (transform->name.substr(0, 9) == "Icosphere") { //TODO: change name
		// 	roughness = (transform->position.y + 10.0f) / 18.0f;
		// }

		// printf("-- Transform name: %s\n", transform->name.c_str());
		for (size_t i = 0; i < named_textures.size(); ++i)
		{
			// printf("Checking prefix: %s\n", named_textures[i].prefix.c_str());
			if (transform->name.rfind(named_textures[i].prefix, 0) == 0)
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

		return ret; });

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
} fb;

Load< MeshBuffer > enemy_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	return new MeshBuffer(data_path("enemy.pnct"));
});

Load< BoneInfluenceBuffer > enemy_infls(LoadTagDefault, []() -> BoneInfluenceBuffer const * {
	return new BoneInfluenceBuffer(data_path("enemy.infl"));
});

Load<SkeletonBuffer> enemy_skeletons(LoadTagDefault, []() {
    return new SkeletonBuffer(data_path("enemy.skel"));
});

Load< MeshBuffer > deer_human_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	return new MeshBuffer(data_path("deer_human.pnct"));
});

Load< BoneInfluenceBuffer > deer_human_infls(LoadTagDefault, []() -> BoneInfluenceBuffer const * {
	return new BoneInfluenceBuffer(data_path("deer_human.infl"));
});

Load<SkeletonBuffer> deer_human_skeletons(LoadTagDefault, []() {
    return new SkeletonBuffer(data_path("deer_human.skel"));
});

Load< AnimationBuffer< Skeleton::BoneTransform > > human_animations(LoadTagDefault, []() -> AnimationBuffer< Skeleton::BoneTransform > const * {
	return new AnimationBuffer< Skeleton::BoneTransform >(data_path("human.anim"));
});

static uint64_t seed_counter = 0;

void make_civilian(
	PlayMode *playmode,
	const std::string &civilian_name,
	const glm::vec3 &location
) {
	static std::map< std::string, std::unique_ptr< MeshBuffer > > mesh_cache;
	static std::map< std::string, std::unique_ptr< BoneInfluenceBuffer > > infl_cache;
	static std::map< std::string, std::unique_ptr< SkeletonBuffer > > skeleton_cache;
	mesh_cache[civilian_name] = std::make_unique< MeshBuffer >(data_path(civilian_name + ".pnct"));
	infl_cache[civilian_name] = std::make_unique< BoneInfluenceBuffer >(data_path(civilian_name + ".infl"));
	skeleton_cache[civilian_name] = std::make_unique< SkeletonBuffer >(data_path(civilian_name + ".skel"));

	auto &mesh_buffer = mesh_cache[civilian_name];
	auto &infl_buffer = infl_cache[civilian_name];
	auto &skeleton_buffer = skeleton_cache[civilian_name];

	Skeleton const &human_skel = skeleton_buffer->lookup("Human.rigify");

	auto g = [](const Skeleton::BoneTransform &a,
				const Skeleton::BoneTransform &b,
				float t) {
		Skeleton::BoneTransform out;
		out.position = glm::mix(a.position, b.position, t);
		out.rotation = glm::normalize(glm::slerp(a.rotation, b.rotation, t));
		out.scale = glm::mix(a.scale, b.scale, t);
		return out;
	};

	playmode->scene.transforms.emplace_back();
	Scene::Transform *transform = &playmode->scene.transforms.back();
	transform->position = location;
	transform->rotation = glm::quat(1, 0, 0, 0);
	transform->scale = glm::vec3(1.1f);

	Civilian c;
	c.transform = transform;
	c.base_rotation = transform->rotation;
	c.start_pos = glm::vec2(location.x, location.y);
	c.skel = std::make_unique< Skeleton >(human_skel);
	c.anim_buffer = std::make_unique< AnimationBuffer< Skeleton::BoneTransform > >(data_path("human.anim"));
		
	c.graph = AnimationGraph< Skeleton::BoneTransform >(g);
	
	c.graph.add_state(c.anim_buffer->lookup("Stand"));
	c.graph.add_state(c.anim_buffer->lookup("Walk"));
	c.graph.add_state(c.anim_buffer->lookup("Run"));
	c.graph.add_state(c.anim_buffer->lookup("WalkToRun"));
	c.graph.add_state(c.anim_buffer->lookup("RunToWalk"));
	c.graph.add_state(c.anim_buffer->lookup("WalkToStand"));
	c.graph.add_state(c.anim_buffer->lookup("StandToWalk"));
	
	c.rng.seed(std::random_device{}() ^ (++seed_counter));

	std::vector<std::string> renderable_mesh_names;
	for (const auto& mesh_pair : mesh_buffer->meshes) {
		const std::string& mesh_name = mesh_pair.first;
		if (mesh_name.find("WGT-") == 0) continue;
		renderable_mesh_names.push_back(mesh_name);
	}
	c.rigs.clear();
	c.rigs.reserve(renderable_mesh_names.size());
	
	for (const std::string& mesh_name : renderable_mesh_names) {
		const Mesh& mesh = mesh_buffer->lookup(mesh_name);
		c.rigs.emplace_back(std::make_unique< RiggedMesh >(
			mesh_buffer->buffer, infl_buffer->buffer, mesh, *c.skel, &c.graph));
		c.rigs.back()->anim_graph = &c.graph;
		
		playmode->scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = playmode->scene.drawables.back();
		playmode->civilian_drawables.push_back(&drawable);
		
		drawable.pipeline = skinning_deferred_program_pipeline;
		drawable.pipeline.vao = c.rigs.back()->make_vao_for_program(skinning_deferred_program->program);
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
		
		bool found_texture = false;
		
		std::string texture_match_name = mesh_name;
		if (mesh_name == "base") {
			std::string civ_num = "";
			size_t underscore_pos = civilian_name.find('_');
			if (underscore_pos != std::string::npos && underscore_pos + 1 < civilian_name.length()) {
				civ_num = civilian_name.substr(underscore_pos + 1);
				texture_match_name = "base_" + civ_num;
			}
		}
		
		if (texture_match_name != mesh_name) {
			for (size_t i = 0; i < named_textures.size(); ++i) {
				if (texture_match_name == named_textures[i].prefix) {
					if (i < textures->size()) {
						GLuint tex_id = (*textures)[i];
						if (tex_id != 0) {
							drawable.pipeline.textures[0].texture = tex_id;
							drawable.pipeline.textures[0].target = GL_TEXTURE_2D;
							found_texture = true;
							break;
						}
					}
				}
			}
		}
		
		if (!found_texture) {
			for (size_t i = 0; i < named_textures.size(); ++i) {
				if (mesh_name == named_textures[i].prefix || 
				    mesh_name.rfind(named_textures[i].prefix, 0) == 0) {
					if (i < textures->size()) {
						GLuint tex_id = (*textures)[i];
						if (tex_id != 0) {
							drawable.pipeline.textures[0].texture = tex_id;
							drawable.pipeline.textures[0].target = GL_TEXTURE_2D;
							found_texture = true;
							break;
						}
					}
				}
			}
		}
		if (!found_texture) {
			std::cout << "No texture found for civilian '" << civilian_name << "' mesh '" << mesh_name << std::endl;
		}
		
		auto rig_ptr = c.rigs.back().get();
		drawable.pipeline.set_uniforms = [rig_ptr]() {
			rig_ptr->bind_pose_ubo();
			glUniform1f(skinning_deferred_program->ROUGHNESS_float, 0.5f);
		};
	}
	
	playmode->civilians.emplace_back(std::move(c));
}

PlayMode::PlayMode() : scene(*zoo_scene_deferred) {
	//get pointers to transforms for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Player") player = &transform;
		if (transform.name == "Enemy") enemy = &transform;
		if (transform.name == "Final_Deer") final_deer = &transform;
		if (transform.name == "Final_Deer Leg") {
			final_deer_leg = &transform;
			transform.scale = glm::vec3(0.0f); // set invisible initially
		}
		if (transform.name == "Sky") sky = &transform;
		if (transform.name == "Gate") gate = &transform;
		if (transform.name == "Gate_L") gate_L = &transform;
		if (transform.name == "Gate_R") gate_R = &transform;
		if (transform.name == "Collider_Gate") gate_collider = &transform;
		if (transform.name == "Collider_Deer Fence") deer_fence_collider = &transform;
		if (transform.name == "Collider_Zoo Fence Near") zoo_fence_near_collider = &transform;
		if (transform.name == "Collider_Zoo Fence Far") zoo_fence_far_collider = &transform;
		// if (transform.name == "Small House Main") small_house = &transform;
	
		// if (transform.name.rfind("Wood Cylinder", 0) == 0)
		// {
		// 	cylinders.push_back(&transform);
		// 	// printf("Found tree: %s\n", transform.name.c_str());
		// }
	}
	if (player == nullptr) throw std::runtime_error("Player not found.");
	if (enemy == nullptr) throw std::runtime_error("enemy not found.");
	if (final_deer == nullptr) throw std::runtime_error("final_deer not found.");
	if (final_deer_leg == nullptr) throw std::runtime_error("final_deer_leg not found.");
	if (sky == nullptr) throw std::runtime_error("sky not found.");
	if (gate == nullptr) throw std::runtime_error("gate not found.");
	if (gate_L == nullptr) throw std::runtime_error("gate_L not found.");
	if (gate_R == nullptr) throw std::runtime_error("gate_R not found.");
	if (gate_collider == nullptr) throw std::runtime_error("gate_collider not found.");
	if (deer_fence_collider == nullptr) throw std::runtime_error("deer_fence_collider not found.");
	if (zoo_fence_near_collider == nullptr) throw std::runtime_error("zoo_fence_near_collider not found.");
	if (zoo_fence_far_collider == nullptr) throw std::runtime_error("zoo_fence_far_collider not found.");

	bg_loop = Sound::loop(*bg_sample, .75f, 0.0f);

	scene.drawables.remove_if([this](Scene::Drawable const &drawable) {
		return drawable.transform == enemy;
	});

	player_base_rotation = player->rotation;

	Skeleton const &enemy_skel = enemy_skeletons->lookup("Human.rigify");

	enemy_skeleton = std::make_unique< Skeleton >(enemy_skel);
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
	enemy_graph.add_state(human_animations->lookup("Stand"));
	enemy_graph.add_state(human_animations->lookup("Walk"));
	enemy_graph.add_state(human_animations->lookup("StandToWalk"));
	enemy_graph.add_state(human_animations->lookup("WalkToStand"));
	enemy_graph.add_state(human_animations->lookup("WalkToDead"));
	
	// Start with Stand animation
	auto stand_it = enemy_graph.states.find("Stand");
	if (stand_it != enemy_graph.states.end()) {
		enemy_graph.current_state = &stand_it->second;
		enemy_graph.playback = 0.0f;
		enemy_graph.keyframe_index = 0;
	}
	enemy_state = ENEMY_STAND;
	
	std::vector<std::string> renderable_mesh_names;
	for (const auto& mesh_pair : enemy_meshes->meshes) {
		const std::string& mesh_name = mesh_pair.first;
		if (mesh_name.find("WGT-") == 0) continue;
		renderable_mesh_names.push_back(mesh_name);
	}
	enemy_rigs.clear();
	enemy_rigs.reserve(renderable_mesh_names.size());
	enemy_drawables.clear();
	enemy_drawables.reserve(renderable_mesh_names.size());
	
	for (const std::string& mesh_name : renderable_mesh_names) {
		const Mesh& mesh = enemy_meshes->lookup(mesh_name);
		enemy_rigs.emplace_back(std::make_unique< RiggedMesh >(
			enemy_meshes->buffer, enemy_infls->buffer, mesh, *enemy_skeleton, &enemy_graph));
		enemy_rigs.back()->anim_graph = &enemy_graph;

		scene.drawables.emplace_back(enemy);
		Scene::Drawable &drawable = scene.drawables.back();
		enemy_drawables.push_back(&drawable);
		drawable.pipeline = skinning_deferred_program_pipeline;
		drawable.pipeline.vao = enemy_rigs.back()->make_vao_for_program(skinning_deferred_program->program);
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
		
		if (mesh_name == "fedora_cocked") {
			enemy_hat_drawable = &drawable;
		}
		if (mesh_name == "base") {
			enemy_base_drawable = &drawable;
		}
		
		std::vector<std::string> keep_visible = {
			"fedora_cocked",
			"male_elegantsuit01",
			"punkduck_tennis_shoes",
			"toigo_ankle_boots_male"
		};
		bool keep_visible_mesh = false;
		for (const std::string& keep_name : keep_visible) {
			if (mesh_name == keep_name) {
				keep_visible_mesh = true;
				break;
			}
		}
		if (!keep_visible_mesh) {
			enemy_fade_drawables.push_back({&drawable, mesh.count});
		}
		
		bool found_texture = false;
		for (size_t i = 0; i < named_textures.size(); ++i) {
			if (mesh_name == named_textures[i].prefix || mesh_name.rfind(named_textures[i].prefix, 0) == 0) {
				if (i < textures->size()) {
					GLuint tex_id = (*textures)[i];
					if (tex_id != 0) {
						drawable.pipeline.textures[0].texture = tex_id;
						drawable.pipeline.textures[0].target = GL_TEXTURE_2D;
						found_texture = true;
						break;
					}
				}
			}
		}
		if (!found_texture) {
			std::cout << "No texture found for enemy mesh '" << mesh_name << std::endl;
		}
		
		auto rig_ptr = enemy_rigs.back().get();
		drawable.pipeline.set_uniforms = [rig_ptr]() {
			rig_ptr->bind_pose_ubo();
			glUniform1f(skinning_deferred_program->ROUGHNESS_float, 0.5f);
		};
	}

	// Load deer human
	Skeleton const &deer_human_skel = deer_human_skeletons->lookup("Human.rigify");
	deer_human_skeleton = std::make_unique< Skeleton >(deer_human_skel);
	
	deer_human_graph = AnimationGraph< Skeleton::BoneTransform >(g);
	deer_human_graph.add_state(human_animations->lookup("Stand"));
	deer_human_graph.add_state(human_animations->lookup("Walk"));
	deer_human_graph.add_state(human_animations->lookup("StandToWalk"));
	deer_human_graph.add_state(human_animations->lookup("WalkToStand"));
	auto deer_stand_it = deer_human_graph.states.find("Stand");
	if (deer_stand_it != deer_human_graph.states.end()) {
		deer_human_graph.current_state = &deer_stand_it->second;
		deer_human_graph.playback = 0.0f;
		deer_human_graph.keyframe_index = 0;
	}
		
	std::vector<std::string> deer_human_mesh_names;
	for (const auto& mesh_pair : deer_human_meshes->meshes) {
		const std::string& mesh_name = mesh_pair.first;
		if (mesh_name.find("WGT-") == 0) continue;
		deer_human_mesh_names.push_back(mesh_name);
	}
	deer_human_rigs.clear();
	deer_human_rigs.reserve(deer_human_mesh_names.size());
	deer_human_drawables.clear();
	deer_human_drawables.reserve(deer_human_mesh_names.size());
	deer_human_original_counts.clear();
	deer_human_original_counts.reserve(deer_human_mesh_names.size());
	
	for (const std::string& mesh_name : deer_human_mesh_names) {
		const Mesh& mesh = deer_human_meshes->lookup(mesh_name);
		deer_human_rigs.emplace_back(std::make_unique< RiggedMesh >(
			deer_human_meshes->buffer, deer_human_infls->buffer, mesh, *deer_human_skeleton, &deer_human_graph));
		deer_human_rigs.back()->anim_graph = &deer_human_graph;

		scene.drawables.emplace_back(player);
		Scene::Drawable &drawable = scene.drawables.back();
		deer_human_drawables.push_back(&drawable);
		deer_human_original_counts.push_back(mesh.count);
		drawable.pipeline = skinning_deferred_program_pipeline;
		drawable.pipeline.vao = deer_human_rigs.back()->make_vao_for_program(skinning_deferred_program->program);
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
		
		for (size_t i = 0; i < named_textures.size(); ++i) {
			if (mesh_name == named_textures[i].prefix || mesh_name.rfind(named_textures[i].prefix, 0) == 0) {
				if (i < textures->size()) {
					GLuint tex_id = (*textures)[i];
					if (tex_id != 0) {
						drawable.pipeline.textures[0].texture = tex_id;
						drawable.pipeline.textures[0].target = GL_TEXTURE_2D;
						break;
					}
				}
			}
		}
		
		auto rig_ptr = deer_human_rigs.back().get();
		drawable.pipeline.set_uniforms = [rig_ptr]() {
			rig_ptr->bind_pose_ubo();
			glUniform1f(skinning_deferred_program->ROUGHNESS_float, 0.5f);
		};
	}

	// populate civilians
	std::mt19937 civilians_rng{std::random_device{}()};
	glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_00", center + glm::vec3(x, y, 0.3f));
	}
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_01", center + glm::vec3(x, y, 0.3f));
	}
	center = glm::vec3(0.0f, 20.0f, 0.0f);
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_02", center + glm::vec3(x, y, 0.3f));
	}
	center = glm::vec3(0.0f, 40.0f, 0.0f);
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_03", center + glm::vec3(x, y, 0.3f));
	}
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_04", center + glm::vec3(x, y, 0.3f));
	}
	center = glm::vec3(0.0f, 60.0f, 0.0f);
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_05", center + glm::vec3(x, y, 0.3f));
	}
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_06", center + glm::vec3(x, y, 0.3f));
	}
	center = glm::vec3(0.0f, 50.0f, 0.0f);
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_07", center + glm::vec3(x, y, 0.3f));
	}
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_08", center + glm::vec3(x, y, 0.3f));
	}
	center = glm::vec3(10.0f, 50.0f, 0.0f);
	{
		float x = rand(civilians_rng, -5.0f, 5.0f);
		float y = rand(civilians_rng, -5.0f, 5.0f);
		make_civilian(this, "civilian_09", center + glm::vec3(x, y, 0.3f));
	}

	for (auto &civilian : civilians) {
		for (auto& rig : civilian.rigs) {
			rig->anim_graph = &civilian.graph;
		}
		
		// Randomize initial state
		std::uniform_real_distribution<float> state_dist(0.0f, 1.0f);
		float state_r = state_dist(civilian.rng);		
		std::string initial_state = "Stand";
		if (state_r < 0.33f) {
			initial_state = "Stand";
		} else if (state_r < 0.66f) {
			initial_state = "Walk";
		} else {
			initial_state = "Run";
		}
		auto state_it = civilian.graph.states.find(initial_state);
		civilian.graph.current_state = &state_it->second;
		if (initial_state == "Stand") {
			civilian.state = Civilian::STAND;
		} else if (initial_state == "Walk") {
			civilian.state = Civilian::WALK;
		} else if (initial_state == "Run") {
			civilian.state = Civilian::RUN;
		}
		
		// Randomize initial direction
		float yaw = rand(civilian.rng, 0.0f, 2.0f * 3.14f);
		civilian.transform->rotation = glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f));
		civilian.base_rotation = civilian.transform->rotation;

		// assign sounds to animations
		civilian.graph.playback = rand(civilians_rng, 0.f, civilian.graph.current_state->animation.get_anim_length());
		for (float time : { .1666f, .7083f }) {
			civilian.anim_buffer->animations.find("Walk")->second.add_event(time, [&civilian]() {
				std::mt19937 dev;
				size_t i = (size_t)(rand(dev, 0.f, (float)footstep_sounds->size()));
				Sound::play_3D(footstep_sounds->at(i), .8f, civilian.transform->position, 5.f);
			});
		}

		for (float time : { .0833f, .5416f }) {
			civilian.anim_buffer->animations.find("Run")->second.add_event(time, [&civilian]() {
				std::mt19937 dev;
				size_t i = (size_t)(rand(dev, 0.f, (float)footstep_sounds->size()));
				Sound::play_3D(footstep_sounds->at(i), .8f, civilian.transform->position, 5.f);
			});
		}
	}

	// Gate
	if (gate_rig) {
		scene.drawables.emplace_back(gate);
		Scene::Drawable &gate_drawable = scene.drawables.back();
		gate_drawable.pipeline = skinning_program_pipeline;
		gate_drawable.pipeline.vao =
			gate_rig->make_vao_for_program(skinning_program->program);
		gate_drawable.pipeline.type = gate_rig->mesh.type;
		gate_drawable.pipeline.start = gate_rig->mesh.start;
		gate_drawable.pipeline.count = gate_rig->mesh.count;
	}

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

	blood_pg.set_texture(Texture::load_from_png(data_path("textures/blood.png")));
	blood_pg.set_angle_range(0.f, glm::radians(360.f));
	blood_pg.set_lifetime_range(.15f, .35f);
	blood_pg.set_size_range(.1f, .25f);
	blood_pg.set_speed_range(5.f, 8.f);
	blood_pg.set_spawn_rate(0.f);

	dust_pg.set_texture(Texture::load_from_png(data_path("textures/dust.png")));
	dust_pg.set_angle_range(0.f, glm::radians(360.f));
	dust_pg.set_lifetime_range(1.f, 1.5f);
	dust_pg.set_size_range(.35f, .75f);
	dust_pg.set_speed_range(.75f, 1.25f);
	dust_pg.set_spawn_rate(0.f);
	dust_pg.spawn_offset = glm::vec3(0.f, 0.f, .15f);

	dust_pg.transform.parent = player;
}

void PlayMode::trigger_game_over() {
	if (game_over) return; // idempotent
	
	game_over = true;
	Sound::stop_all_samples();
}

void PlayMode::trigger_game_success() {
	if (game_success) return; // idempotent
	game_success = true;
}

void PlayMode::enemy_to_dead() {
	if (!enemy || !enemy_alive || enemy_collapsing) return;
	enemy_transitioning_to_dead = true;
	
	auto set_state = [&](const std::string &name) {
		auto it = enemy_graph.states.find(name);
		if (it != enemy_graph.states.end()) {
			enemy_graph.current_state = &it->second;
			enemy_graph.playback = 0.0f;
			enemy_graph.keyframe_index = 0;
		}
	};
	
	if (enemy_state == ENEMY_STAND) {
		set_state("StandToWalk");
		enemy_state = ENEMY_BETWEEN;
	} else if (enemy_state == ENEMY_WALK) {
		set_state("WalkToDead");
		enemy_state = ENEMY_BETWEEN;
	}
}

PlayMode::~PlayMode() {
	/*	// NEW: 释放 deer UI 资源
	if (deer_ui_tex) glDeleteTextures(1, &deer_ui_tex);
	if (deer_ui_vbo) glDeleteBuffers(1, &deer_ui_vbo);
	if (deer_ui_vao) glDeleteVertexArrays(1, &deer_ui_vao);
	*/


}

void PlayMode::start_footstep_loop()
{
	if (!footstep_loop)
	{
		footstep_loop = Sound::loop(*footstep_sample, 2.0f, 0.0f);
	}
}

void PlayMode::stop_footstep_loop()
{
	if (footstep_loop &&
		!left.pressed && !right.pressed &&
		!up.pressed && !down.pressed)
	{
		footstep_loop->stop();
		footstep_loop.reset();
	}
}

void PlayMode::start_stalking_loop()
{
	if (!stalking_loop)
	{
		stalking_loop = Sound::loop(*stalking_sample, 1.0f, 0.0f);
	}
}

void PlayMode::stop_stalking_loop()
{
	if (stalking_loop &&
		!left.pressed && !right.pressed &&
		!up.pressed && !down.pressed)
	{
		stalking_loop->stop();
		stalking_loop.reset();
	}
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//update 11/24 Alex Ding
	// --- MAIN MENU INPUT ---
    if (screen_state == ScreenState::MENU) {
        if (evt.type == SDL_EVENT_KEY_DOWN) {
            // Press ENTER to start the game
            if (evt.key.key == SDLK_RETURN || evt.key.key == SDLK_SPACE) {
                screen_state = ScreenState::PLAYING;
				Sound::play(*start_sample, 1.0f, 1.0f);

                // (Optional) capture mouse when the game actually starts:
                SDL_SetWindowRelativeMouseMode(Mode::window, true);

                return true;
            }

            // Press ESC to quit the whole game
            if (evt.key.key == SDLK_ESCAPE) {
                SDL_Event quit;
                quit.type = SDL_EVENT_QUIT;  // SDL3 style
                SDL_PushEvent(&quit);
                return true;
            }
        }

        // while in menu, ignore other events
        return false;
    }

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		}else if (evt.key.key == SDLK_R) {
			// restart the game with a fresh PlayMode
			if (game_over) {
				Mode::set_current(std::make_shared< PlayMode >());
			}
			return true;
		} else if (game_success) {
			return false;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			start_footstep_loop();
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			start_footstep_loop();
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			start_footstep_loop();
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			start_footstep_loop();
			return true;
		}else if (evt.key.key == SDLK_SPACE) {
			// Start dash if unlocked, not already dashing, and off cooldown
			if (dash_skill && !dashing && dash_cooldown_timer <= 0.0f && !game_over) {
				// Dash direction: camera-forward (on ground plane)
				glm::mat4x3 cam_frame = player->make_parent_from_local();
				glm::vec3 frame_forward = -cam_frame[1]; // consistent with your WASD forward
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

				// 1. Pick random ID from list 1–4
				std::uniform_int_distribution<int> dist(0, int(attraction_ids.size()) - 1);
				int id = attraction_ids[dist(rng)];

				// 2. Play sound depending on ID
				switch (id) {
					case 1:
						Sound::play(*attraction_voice_1, 1.0f, 1.0f);
						break;
					case 2:
						Sound::play(*attraction_voice_2, 1.0f, 1.0f);
						break;
					case 3:
						Sound::play(*attraction_voice_3, 1.0f, 1.0f);
						break;
					case 4:
						Sound::play(*attraction_voice_4, 1.0f, 1.0f);
						break;
				}
				for (auto &civ : civilians) {
					if (!civ.transform) continue;

					float dist_xy = glm::length(
						glm::vec2(
							civ.transform->position.x - player->position.x,
							civ.transform->position.y - player->position.y
						)
					);

					// choose radius you like (25 is pretty big)
					if (dist_xy < 25.0f) {
						civ.being_pulled = true;
						civ.pull_target = player->position;
					}
				}				
				// cooldown
				attraction_cooldown_timer = attraction_cooldown;

				// --- Make nearby civilians walk toward the player ---
				for (auto &civ : civilians) {
					// range: adjust 20.0f if needed
					float dist = glm::length(player->position - civ.transform->position);
					if (dist < 25.0f) {
						civ.being_pulled = true;
						civ.pull_target = player->position;   // goal
					}
				}
			}
			return true;
		}
		else if (evt.key.key == SDLK_P && !gate_can_open)
		{
			Sound::play_3D(*gate_open_sample, 1.2f, gate->position, 30.0f);

    		gate_anim_playing = true; 
			gate_can_open = true; // TODO: need a condition for this to be true

			gate_rot_t = 0.0f;
			glm::vec3 z_axis(0.0f, 0.0f, 1.0f);

			gate_L_start = gate_L->rotation;
			gate_R_start = gate_R->rotation;

			float deg = 135.0f;
			gate_L_end = gate_L_start * glm::angleAxis(glm::radians(deg), z_axis);
			gate_R_end = gate_R_start * glm::angleAxis(glm::radians(-deg), z_axis);

			float deg_phase2_L = -7.0f;
			float deg_phase2_R =  7.0f;

			gate_L_final = gate_L_end * glm::angleAxis(glm::radians(deg_phase2_L), z_axis);
			gate_R_final = gate_R_end * glm::angleAxis(glm::radians(deg_phase2_R), z_axis);
			
			return true;
		}
		// TODO: Delete later! For testing purposes
		else if (evt.key.key == SDLK_X)
		{
			is_deer_human = true;
			enemy_to_dead();
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			stop_footstep_loop();
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			stop_footstep_loop();
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			stop_footstep_loop();
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			stop_footstep_loop();
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
			start_stalking_loop();
			return true;
		} else if (evt.button.button == SDL_BUTTON_LEFT) {
			if (execution_mode && execution_target) {
				Sound::play(*attack_sample, 1.0f, 1.0f);

				// Distance calculation
				glm::vec3 player_pos = camera->transform->make_world_from_local()[3];
				glm::vec3 civ_pos    = execution_target->make_world_from_local()[3];

				float dist = glm::length(civ_pos - player_pos);
				if (dist <= execution_range) {
					// === EXECUTION SUCCESS ON CIVILIAN ===
					Sound::play(*enemy_die_sample, 1.0f, 1.0f);
					
					execution_mode = false;
					stalk_charge = 0.0f;

					++kill_count;

					blood_pg.burst_at(execution_target->position + glm::vec3(0.f, .5f, 0.f), 20);

					// hide the civilian visually
					execution_target->scale = glm::vec3(0.0f);
					execution_target->position.z = -100.0f;

					// clear current target so we don't keep reusing it
					execution_target = nullptr;
					if (!dash_skill && kill_count >= 1) {
						dash_skill = true;
						dash_hint_active = true;
						dash_hint_timer = 2.0f;   // show "PRESS SPACE TO DASH" for 2 seconds
					}

					// Unlock attraction sound on second kill (only once)
					if (!attraction_ability && kill_count >= 2) {
						attraction_ability = true;
						sound_hint_active = true;
						sound_hint_timer = 2.0f;  // show "PRESS G TO Make Attraction" for 2 seconds
					}

					if (!pass_hint_active && kill_count >= 5) {
						pass_hint_active = true;
						is_deer_human = true;
					}


					if (deer_stage == 0) {
						final_deer->scale = glm::vec3(0.0f);  // hide original deer
						final_deer_leg->scale = glm::vec3(0.7f);
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
			target_fovy = base_fovy * 2.1f; // restore zoom
			stop_stalking_loop();
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
			glm::vec2 motion = glm::vec2(evt.motion.xrel / float(window_size.y),
										 -evt.motion.yrel / float(window_size.y));

			cam->update_camera(motion * camera->fovy);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	if (screen_state == ScreenState::MENU) {
			return;
		}

	if (game_over) {
		// Optional: keep camera/UI effects, but block gameplay logic
		// camera->fovy = glm::mix(camera->fovy, target_fovy, 1.0f - std::exp(-elapsed * zoom_speed));
		return;
	}

	// Sound
	if (footstep_loop)
	{
		footstep_loop->set_position(player->position);
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
	if (attraction_cooldown_timer > 0.0f) {
    attraction_cooldown_timer = std::max(0.0f, attraction_cooldown_timer - elapsed);
	}
	//11/24 Update Alex Ding
	//Tutor word
	if (dash_hint_active) {
    dash_hint_timer -= elapsed;
    if (dash_hint_timer <= 0.0f) {
        dash_hint_active = false;
    	}
	}
	if (sound_hint_active) {
    sound_hint_timer -= elapsed;
    if (sound_hint_timer <= 0.0f) {
        sound_hint_active = false;
    	}
	}
	if (pass_hint_active) {
    pass_hint_timer -= elapsed;
    if (pass_hint_timer <= 0.0f) {
        pass_hint_active = false;
    	}
	}
	//
	if (enemy_fade_timer > 0.0f) {
		enemy_fade_timer = std::max(0.0f, enemy_fade_timer - elapsed);
		float fade_progress = 1.0f - (enemy_fade_timer / enemy_fade_duration);
		for (auto &[drawable, original_count] : enemy_fade_drawables) {
			if (drawable) {
				float remaining = 1.0f - fade_progress;
				drawable->pipeline.count = (uint32_t)(original_count * remaining);
			}
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
	enemy->scale = glm::vec3(1.5f);
	enemy_graph.update(elapsed);
	
	// Update deer human
	if (is_deer_human) {
		final_deer->scale = glm::vec3(0.0f);
		final_deer_leg->scale = glm::vec3(0.0f);
		player->scale = glm::vec3(1.3f);	
		bool player_is_moving = (left.pressed || right.pressed || up.pressed || down.pressed) && !dashing;
		
		// Update animation state
		auto set_deer_state = [&](const std::string& state_name) {
			auto state_it = deer_human_graph.states.find(state_name);
			if (state_it != deer_human_graph.states.end() && deer_human_graph.current_state != &state_it->second) {
				// Check if we need a transition
				if (deer_human_moving && state_name == "Stand") {
					auto walk_to_stand_it = deer_human_graph.states.find("WalkToStand");
					if (walk_to_stand_it != deer_human_graph.states.end()) {
						deer_human_graph.current_state = &walk_to_stand_it->second;
					} else {
						deer_human_graph.current_state = &state_it->second;
					}
					deer_human_graph.playback = 0.0f;
					deer_human_graph.keyframe_index = 0;
				} else if (!deer_human_moving && state_name == "Walk") {
					auto stand_to_walk_it = deer_human_graph.states.find("StandToWalk");
					if (stand_to_walk_it != deer_human_graph.states.end()) {
						deer_human_graph.current_state = &stand_to_walk_it->second;
					} else {
						deer_human_graph.current_state = &state_it->second;
					}
					deer_human_graph.playback = 0.0f;
					deer_human_graph.keyframe_index = 0;
				} else {
					deer_human_graph.current_state = &state_it->second;
				}
			}
		};
		
		// Handle transition
		if (deer_human_graph.current_state) {
			auto& cur_state = *deer_human_graph.current_state;
			if (cur_state.animation.name == "StandToWalk") {
				if (deer_human_graph.playback >= cur_state.animation.get_anim_length()) {
					set_deer_state("Walk");
				}
			} else if (cur_state.animation.name == "WalkToStand") {
				if (deer_human_graph.playback >= cur_state.animation.get_anim_length()) {
					set_deer_state("Stand");
				}
			}
		}
		
		if (player_is_moving && !deer_human_moving) {
			set_deer_state("Walk");
			deer_human_moving = true;
		} else if (!player_is_moving && deer_human_moving) {
			set_deer_state("Stand");
			deer_human_moving = false;
		}
		
		deer_human_graph.update(elapsed);
		
		for (auto& rig : deer_human_rigs) {
			rig->update(elapsed);
		}
		
		for (size_t i = 0; i < deer_human_drawables.size() && i < deer_human_original_counts.size(); ++i) {
			if (deer_human_drawables[i]) {
				deer_human_drawables[i]->pipeline.count = deer_human_original_counts[i];
			}
		}
	} else {
		for (Scene::Drawable *drawable : deer_human_drawables) {
			if (drawable) {
				drawable->pipeline.count = 0;
			}
		}
	}

	for (auto& rig : enemy_rigs) {
		rig->update(elapsed);
	}

	if (gate_anim_playing)  {
		gate_rot_t += elapsed;
		float percent_played = gate_rot_t / (gate_rot_duration_1 + gate_rot_duration_2);
		float phase1_end = gate_rot_duration_1 / (gate_rot_duration_1 + gate_rot_duration_2);

		if (percent_played < phase1_end)
		{
			// Open
			float t1 = percent_played / phase1_end; // 0..1
			gate_L->rotation = glm::slerp(gate_L_start, gate_L_end, t1);
			gate_R->rotation = glm::slerp(gate_R_start, gate_R_end, t1);
		}
		else
		{
			// Swing back a little
			float t2 = (percent_played - phase1_end) / (1.0f - phase1_end); // 0..1 over second 6 seconds
			if (t2 > 1.0f)
				t2 = 1.0f;

			gate_L->rotation = glm::slerp(gate_L_end, gate_L_final, t2);
			gate_R->rotation = glm::slerp(gate_R_end, gate_R_final, t2);
		}

		// stop after total duration
		if (percent_played >= 1.0f)
		{
			gate_anim_playing = false;
		}
	}

	camera->fovy = glm::mix(camera->fovy, target_fovy, 1.0f - std::exp(-elapsed * zoom_speed));

	// --- Player movement (WASD, relative to camera) ---
	{
		dust_pg.set_spawn_rate(0.f);
		if (dashing)
		{
			player->position += dash_dir * dash_speed * elapsed;
			dust_pg.set_spawn_rate(.1f);
		} else {
			constexpr float PlayerSpeed = 7.5f;
			constexpr float RotationSpeed = .3f; // interp weight : between [0, 1]
			glm::vec2 move = glm::vec2(0.0f);
			if (left.pressed && !right.pressed)
				move.x = -1.0f;
			if (!left.pressed && right.pressed)
				move.x = 1.0f;
			if (down.pressed && !up.pressed)
				move.y = -1.0f;
			if (!down.pressed && up.pressed)
				move.y = 1.0f;

			if (move != glm::vec2(0.0f)) {
				move = glm::normalize(move);

				dust_pg.set_spawn_rate(.1f);

				// compute new player rotation relative to camera
				glm::vec3 forward_xy = glm::vec3 (
					std::sinf(cam->yaw), 
					-std::cos(cam->yaw), 
					0.f);
				glm::vec3 right_xy = glm::vec3 (
					forward_xy.y, 
					-forward_xy.x, 
					0.f);
				
				glm::vec3 target_dir = -glm::normalize(
					move.x * right_xy + move.y * forward_xy
				);

				float target_yaw = std::atan2f(-target_dir.x, target_dir.y);
				
				// --! our axis are different than glm expected !--
				float current_pitch = glm::pitch(player->rotation);
				float current_roll = glm::yaw(player->rotation);
				// float current_yaw = glm::roll(player->rotation);
				
				// slerp rotation
				player->rotation = glm::slerp(
					player->rotation,
					glm::quat( glm::vec3(current_pitch, current_roll, target_yaw) ),
					RotationSpeed
				);

				// get player forward
				glm::vec3 player_forward = player->rotation * -glm::vec3(0.f, 1.f, 0.f);

				glm::vec3 new_pos = player->position + player_forward * PlayerSpeed * player_speed_factor * elapsed;

				// printf("Trying move to: %.2f, %.2f, %.2f\n", new_pos.x, new_pos.y, new_pos.z);

				CollisionHits hits = query_world_collisions(
					new_pos,
					gate_can_open ? nullptr : gate_collider,
					deer_fence_collider,
					zoo_fence_near_collider,
					zoo_fence_far_collider);

				if (hits.escaped())
				{
					trigger_game_success();
				}
				else if (!hits.any())
				{
					player->position = new_pos;
				}
				cam->update_camera(glm::vec2(0.f));
			}
		}
	}

	// --- Enemy on-screen check (clip-space) ---
	enemy_on_screen = false;
	execution_target = nullptr;
	if (!civilians.empty()) {
		glm::mat4 clip_from_world =
			camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());

		// pick the first civilian that is on-screen, or tweak to pick the closest
		for (auto &civilian : civilians) {
			// NOTE: adjust this line to however your Civilian stores its transform.
			// e.g. if Civilian has `Scene::Transform *transform;`, this is correct:
			Scene::Transform *t = civilian.transform;
			if (!t) continue;

			glm::vec3 c_world = t->make_world_from_local()[3];
			glm::vec4 clip = clip_from_world * glm::vec4(c_world, 1.0f);
			if (clip.w <= 0.0f) continue;

			glm::vec3 ndc = glm::vec3(clip) / clip.w; // [-1,1]
			bool on_screen =
				(ndc.x >= -1.0f && ndc.x <= 1.0f &&
				ndc.y >= -1.0f && ndc.y <= 1.0f);

			if (on_screen) {
				enemy_on_screen = true;           // reused flag name
				execution_target = t;             // remember which civilian we’re aiming at
				break;                            // stop at the first on-screen civilian
			}
		}
	}

	// --- Stalk bar charge/decay (depends on enemy on-screen visibility) ---
	if (stalking && enemy_on_screen && enemy_visible) {
		float prev_charge = stalk_charge;
		stalk_charge += stalk_charge_rate * elapsed;

		if (prev_charge < 1.0f && stalk_charge >= 1.0f)
		{
			stalk_charge = 1.0f;
			execution_mode = true;
			Sound::play(*stalking_completed_sample, 0.8f, 1.0f);
		}
		else if (stalk_charge > 1.0f)
		{
			stalk_charge = 1.0f;
		}

		// force pitch and player model to look towards stalk direction
		//  - pitch is forced since enemy pos does not vary vertically
		glm::vec3 to_enemy = enemy->position - player->position;
		
		// control for position of enemy center on screen
		float target_pitch_ratio = 1.f + .05f;
		float target_pitch = (std::atan2f(to_enemy.z, std::sqrtf(to_enemy.x * to_enemy.x + to_enemy.y * to_enemy.y)) 
			+ cam->pitch_range.x / 2.f) * target_pitch_ratio;

		// smooth force camera pitch by lerp
		cam->pitch = cam->pitch * .6f + target_pitch * .4f;
	} 

	// --- Enemy sensing: FOV + distance (+ optional LOS hook) ---
	//2025/11/22 update
	being_watched = false;
	watching_civilian = nullptr;
	for (auto &civ : civilians) {
		civ.watching_player = false;
	}

	if (player) {
		for (auto &civ : civilians) {
			if (!civ.transform) continue;

			glm::mat4x3 c_world = civ.transform->make_world_from_local();
			glm::vec3 c_pos     = c_world[3];
			glm::vec3 c_forward = -glm::vec3(c_world[1]); // -Y is "forward"

			glm::vec3 to_player3 = player->position - c_pos;
			float dist = glm::length(to_player3);
			if (dist <= 0.0001f || dist > enemy_view_distance) continue;

			glm::vec3 dir = to_player3 / dist;
			float cos_half_fov = std::cos(glm::radians(enemy_fov_deg * 0.5f));
			float cos_theta    = glm::dot(glm::normalize(c_forward), dir);

			bool in_fov = (cos_theta > cos_half_fov) && (glm::dot(c_forward, to_player3) > 0.0f);

			// LOS hook (currently always unblocked):
			auto occluded_civ_to_player = [&]() -> bool {
				// TODO: real ray/occlusion test if you want
				return false;
			};
			bool blocked = occluded_civ_to_player();

			if (in_fov && !blocked) {
				being_watched = true;
				watching_civilian = &civ;     // remember this civilian
				civ.watching_player = true;   // put them in watch mode (no movement)
				break; // any one civilian is enough
			}
		}
	}
	//2025/11/22 update
	// --- Latch logic (sticky "seeing" state with grace timeout) ---
	//2025/11/22 update
	if (being_watched) {
		watched_accum += elapsed;
		if (watched_accum >= watch_to_gameover) {
			trigger_game_over();
		}
	} else {
			// continuous requirement: reset if not watched this frame
			watched_accum = 0.0f;
	}

	// --- Enemy behavior: Stand-and-watch vs Patrol ---

	if (enemy && enemy_alive && !enemy_collapsing) {
		auto set_state = [&](const std::string &name) {
			auto it = enemy_graph.states.find(name);
			if (it != enemy_graph.states.end()) {
				enemy_graph.current_state = &it->second;
				enemy_graph.playback = 0.0f;
				enemy_graph.keyframe_index = 0;
			}
		};
		
		if (enemy_state == ENEMY_BETWEEN && enemy_graph.current_state) {
			const auto &anim = enemy_graph.current_state->animation;
			if (!anim.loop && enemy_graph.playback >= anim.get_anim_length()) {
				if (anim.name == "StandToWalk") {
					if (enemy_transitioning_to_dead) {
						set_state("WalkToDead");
						enemy_state = ENEMY_BETWEEN;
						auto walk_to_dead_it = enemy_graph.states.find("WalkToDead");
						if (walk_to_dead_it != enemy_graph.states.end()) {
							float anim_duration = walk_to_dead_it->second.animation.get_anim_length();
							// Fade halfway through animation
							enemy_fade_duration = anim_duration * 0.5f;
							enemy_fade_timer = anim_duration * 0.5f;
						}
					} else {
						set_state("Walk");
						enemy_state = ENEMY_WALK;
					}
				} else if (anim.name == "WalkToStand") {
					if (enemy_transitioning_to_dead) {
						set_state("StandToWalk");
						enemy_state = ENEMY_BETWEEN;
					} else {
						set_state("Stand");
						enemy_state = ENEMY_STAND;
					}
				} else if (anim.name == "WalkToDead") {
					enemy_transitioning_to_dead = false;
					enemy_alive = false;
					enemy_graph.playback = anim.get_anim_length();
				}
			}
		}
		
		if (enemy_transitioning_to_dead && enemy_state == ENEMY_WALK) {
			set_state("WalkToDead");
			enemy_state = ENEMY_BETWEEN;
			auto walk_to_dead_it = enemy_graph.states.find("WalkToDead");
			if (walk_to_dead_it != enemy_graph.states.end()) {
				float anim_duration = walk_to_dead_it->second.animation.get_anim_length();
				// Fade completes at halfway through animation
				enemy_fade_duration = anim_duration * 0.5f;
				enemy_fade_timer = anim_duration * 0.5f;
			}
		}
		
		if (enemy_state != ENEMY_BETWEEN && !enemy_transitioning_to_dead) {
			auto change_state = [&](EnemyState next) {
				if (next == enemy_state) return;
				if (enemy_state == ENEMY_STAND && next == ENEMY_WALK) {
					set_state("StandToWalk");
					enemy_state = ENEMY_BETWEEN;
				} else if (enemy_state == ENEMY_WALK && next == ENEMY_STAND) {
					set_state("WalkToStand");
					enemy_state = ENEMY_BETWEEN;
				} else {
					set_state(next == ENEMY_STAND ? "Stand" : "Walk");
					enemy_state = next;
				}
			};
			
			glm::vec2 to_player_xy(0.0f);
			float to_player_dist = 0.0f;
			if (player) {
				glm::vec3 v = player->position - enemy->position;
				to_player_xy = glm::vec2(v.x, v.y);
				to_player_dist = glm::length(to_player_xy);
			}

			if (watched_latched) {
				change_state(ENEMY_STAND);
				if (to_player_dist > 1e-4f) {
					glm::vec2 dir = to_player_xy / to_player_dist;
					float yaw = std::atan2(dir.x, dir.y);
					glm::quat target_rot = glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f)) * enemy_base_rotation;
					enemy->rotation = glm::slerp(enemy->rotation, target_rot, 1.0f - std::exp(-8.0f * elapsed));
				}
			} else if (!enemy_waypoints.empty()) {
				if (enemy_wait_timer > 0.0f) {
					enemy_wait_timer = std::max(0.0f, enemy_wait_timer - elapsed);
					change_state(ENEMY_STAND);
				} else {
					glm::vec3 target = enemy_waypoints[enemy_wp_idx];
					glm::vec2 to = glm::vec2(target.x - enemy->position.x, target.y - enemy->position.y);
					float dist = glm::length(to);

					if (dist <= enemy_reach_epsilon) {
						enemy_wp_idx = (enemy_wp_idx + 1) % enemy_waypoints.size();
						enemy_wait_timer = enemy_wait_at_point;
						change_state(ENEMY_STAND);
					} else if (dist > 0.0f) {
						change_state(ENEMY_WALK);
						glm::vec2 dir = to / dist;
						float step = std::min(enemy_speed * elapsed, dist);
						enemy->position.x += dir.x * step;
						enemy->position.y += dir.y * step;
						enemy->position.z = 0.3f;
						float yaw = std::atan2(dir.x, dir.y);
						glm::quat target_rot = glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f)) * enemy_base_rotation;
						enemy->rotation = glm::slerp(enemy->rotation, target_rot, 1.0f - std::exp(-8.0f * elapsed));
					}
				}
			}
		}
	}

	// update civilians
	for (auto &civilian : civilians) {
		civilian_update(civilian, elapsed, player);
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

	blood_pg.continuous_update(elapsed);
	dust_pg.continuous_update(elapsed);

	// --- reset one-frame key counts ---
	left.downs = right.downs = up.downs = down.downs = 0;
}


void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//11/24 update Alex Ding
	// --- MAIN MENU SCREEN ---
    if (screen_state == ScreenState::MENU) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, drawable_size.x, drawable_size.y);

        glDisable(GL_DEPTH_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = float(drawable_size.x) / float(drawable_size.y);

        DrawLines lines(glm::mat4(
            1.0f / aspect, 0.0f,        0.0f, 0.0f,
            0.0f,         1.0f,         0.0f, 0.0f,
            0.0f,         0.0f,         1.0f, 0.0f,
            0.0f,         0.0f,         0.0f, 1.0f
        ));

        glm::u8vec4 white = glm::u8vec4(0xff, 0xff, 0xff, 0xff);

        constexpr float H = 0.09f;

        // Title
        lines.draw_text(
            "ZOO ESCAPE",
            glm::vec3(-0.9f, 0.5f, 0.0f),
            glm::vec3(H, 0.0f, 0.0f),
            glm::vec3(0.0f, H, 0.0f),
            white
        );

        // "Play" instruction
        lines.draw_text(
            "Press ENTER to Play",
            glm::vec3(-0.9f, 0.1f, 0.0f),
            glm::vec3(H * 0.6f, 0.0f, 0.0f),
            glm::vec3(0.0f,  H * 0.6f, 0.0f),
            white
        );

        // "Quit" instruction
        lines.draw_text(
            "Press ESC to Quit",
            glm::vec3(-0.9f, -0.1f, 0.0f),
            glm::vec3(H * 0.6f, 0.0f, 0.0f),
            glm::vec3(0.0f,  H * 0.6f, 0.0f),
            white
        );

        glEnable(GL_DEPTH_TEST);
        return; // important: don't draw the 3D scene
    }
	//11/23 Update
	// --- GAME OVER SCREEN: clear everything and only draw text ---
	if (game_over) {
		// draw straight to default framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, drawable_size.x, drawable_size.y);

		// clear color + depth so nothing from the scene remains
		glDisable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // black background
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		float aspect = float(drawable_size.x) / float(drawable_size.y);

		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f,        0.0f, 0.0f,
			0.0f,         1.0f,        0.0f, 0.0f,
			0.0f,         0.0f,        1.0f, 0.0f,
			0.0f,         0.0f,        0.0f, 1.0f
		));

		constexpr float H = 0.18f; // text size

		// Red main title
		glm::u8vec4 red = glm::u8vec4(0xff, 0x00, 0x00, 0xff);
		lines.draw_text("Zoo has been locked",
			glm::vec3(-0.7f, 0.1f, 0.0f),  // a bit above center
			glm::vec3(H, 0.0f, 0.0f),
			glm::vec3(0.0f, H, 0.0f),
			red
		);

		// White instruction below
		glm::u8vec4 white = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
		constexpr float H2 = 0.10f; // slightly smaller text
		lines.draw_text("Press R to restart",
			glm::vec3(-0.35f, -0.15f, 0.0f),  // below the first line
			glm::vec3(H2, 0.0f, 0.0f),
			glm::vec3(0.0f, H2, 0.0f),
			white
		);

		glEnable(GL_DEPTH_TEST);
		return; // IMPORTANT: skip all normal drawing
	}
	//11/23 Update
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);
	glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());
	glm::vec3 eye = camera->transform->make_world_from_local()[3];

	// setup matrices for particles
	glUseProgram(particle_program->program);
	glUniformMatrix4fv(particle_program->CLIP_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(camera->make_projection() * glm::mat4(camera->transform->make_local_from_world())));
	glUniform1fv(particle_program->ASPECT, 1, (GLfloat *)&camera->aspect);
	glUniform1i(particle_program->LIGHT_TYPE_int, 1);
	glUniform3fv(particle_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(particle_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));	
	glUseProgram(0);

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

	glm::mat4 clip_from_world = camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());
	glm::mat4x3 light_from_world = glm::mat4x3(1.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// draw particles
	blood_pg.draw();
	dust_pg.draw();

	for (Scene::Drawable *drawable : enemy_drawables) {
		if (!drawable) continue;
		
		Scene::Drawable::Pipeline const &pipeline = drawable->pipeline;
		
		if (pipeline.program == 0 || pipeline.vao == 0 || pipeline.count == 0) continue;
		glUseProgram(pipeline.program);
		glBindVertexArray(pipeline.vao);
		glm::mat4x3 world_from_object = drawable->transform->make_world_from_local();
		glm::mat4 clip_from_object = clip_from_world * glm::mat4(world_from_object);
		glUniformMatrix4fv(pipeline.CLIP_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(clip_from_object));
		
		glm::mat4x3 light_from_object = light_from_world * glm::mat4(world_from_object);
		glUniformMatrix4x3fv(pipeline.LIGHT_FROM_OBJECT_mat4x3, 1, GL_FALSE, glm::value_ptr(light_from_object));

		glm::mat3 light_from_normal = glm::inverse(glm::transpose(glm::mat3(light_from_object)));
		glUniformMatrix3fv(pipeline.LIGHT_FROM_NORMAL_mat3, 1, GL_FALSE, glm::value_ptr(light_from_normal));
		
		if (pipeline.set_uniforms) pipeline.set_uniforms();
		
		for (uint32_t i = 0; i < Scene::Drawable::Pipeline::TextureCount; ++i) {
			if (pipeline.textures[i].texture != 0) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(pipeline.textures[i].target, pipeline.textures[i].texture);
			}
		}
		
		glDrawArrays(pipeline.type, pipeline.start, pipeline.count);
		
		for (uint32_t i = 0; i < Scene::Drawable::Pipeline::TextureCount; ++i) {
			if (pipeline.textures[i].texture != 0) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(pipeline.textures[i].target, 0);
			}
		}
		glActiveTexture(GL_TEXTURE0);
	}
	
	// Draw deer_human only if active
	if (is_deer_human) {
		for (Scene::Drawable *drawable : deer_human_drawables) {
		
		Scene::Drawable::Pipeline const &pipeline = drawable->pipeline;
		glUseProgram(pipeline.program);
		glBindVertexArray(pipeline.vao);
		
		glm::mat4x3 world_from_object = drawable->transform->make_world_from_local();
		glm::mat4 clip_from_object = clip_from_world * glm::mat4(world_from_object);
		glUniformMatrix4fv(pipeline.CLIP_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(clip_from_object));
		glm::mat4x3 light_from_object = light_from_world * glm::mat4(world_from_object);
		glUniformMatrix4x3fv(pipeline.LIGHT_FROM_OBJECT_mat4x3, 1, GL_FALSE, glm::value_ptr(light_from_object));
		glm::mat3 light_from_normal = glm::inverse(glm::transpose(glm::mat3(light_from_object)));
		glUniformMatrix3fv(pipeline.LIGHT_FROM_NORMAL_mat3, 1, GL_FALSE, glm::value_ptr(light_from_normal));
		pipeline.set_uniforms();
		
		for (uint32_t i = 0; i < Scene::Drawable::Pipeline::TextureCount; ++i) {
			if (pipeline.textures[i].texture != 0) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(pipeline.textures[i].target, pipeline.textures[i].texture);
			}
		}
		
		glDrawArrays(pipeline.type, pipeline.start, pipeline.count);
		
		for (uint32_t i = 0; i < Scene::Drawable::Pipeline::TextureCount; ++i) {
			if (pipeline.textures[i].texture != 0) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(pipeline.textures[i].target, 0);
			}
		}
		glActiveTexture(GL_TEXTURE0);
		}
	}
	
	// Draw civilian
	for (Scene::Drawable *drawable : civilian_drawables) {
		if (!drawable) continue;
		
		Scene::Drawable::Pipeline const &pipeline = drawable->pipeline;
		
		if (pipeline.program == 0 || pipeline.vao == 0 || pipeline.count == 0) continue;
		glUseProgram(pipeline.program);
		glBindVertexArray(pipeline.vao);
		
		glm::mat4x3 world_from_object = drawable->transform->make_world_from_local();
		glm::mat4 clip_from_object = clip_from_world * glm::mat4(world_from_object);
		glUniformMatrix4fv(pipeline.CLIP_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(clip_from_object));
		
		glm::mat4x3 light_from_object = light_from_world * glm::mat4(world_from_object);
		glUniformMatrix4x3fv(pipeline.LIGHT_FROM_OBJECT_mat4x3, 1, GL_FALSE, glm::value_ptr(light_from_object));
		glm::mat3 light_from_normal = glm::inverse(glm::transpose(glm::mat3(light_from_object)));
		glUniformMatrix3fv(pipeline.LIGHT_FROM_NORMAL_mat3, 1, GL_FALSE, glm::value_ptr(light_from_normal));
		
		if (pipeline.set_uniforms) pipeline.set_uniforms();
		for (uint32_t i = 0; i < Scene::Drawable::Pipeline::TextureCount; ++i) {
			if (pipeline.textures[i].texture != 0) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(pipeline.textures[i].target, pipeline.textures[i].texture);
			}
		}
		
		glDrawArrays(pipeline.type, pipeline.start, pipeline.count);
		for (uint32_t i = 0; i < Scene::Drawable::Pipeline::TextureCount; ++i) {
			if (pipeline.textures[i].texture != 0) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(pipeline.textures[i].target, 0);
			}
		}
		glActiveTexture(GL_TEXTURE0);
	}
	
	glUseProgram(0);
	glBindVertexArray(0);

	//--- stalking mechanics ---
	if (focus_mode) {
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // stark white background for high contrast
	} else if (execution_mode) {
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f); // red background during execution mode
	} else {
		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	}

	// We already have the lit color on the default framebuffer.
	// Now we only want depth, so disable color writes:
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.
	glDepthMask(GL_TRUE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_DEPTH_BUFFER_BIT); // clears depth only (color is masked off)

	// Draw the scene with your normal pipelines; this will fill depth,
	// but leave the deferred-lit color untouched:
	scene.draw(*camera);

	// Re-enable color writes for later overlays:
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	enemy_visible = false; // default

	if (execution_target) {
		glm::mat4 clip_from_world =
			camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());

		glm::vec3 c_world = execution_target->make_world_from_local()[3];
		glm::vec4 clip = clip_from_world * glm::vec4(c_world, 1.0f);

		if (clip.w > 0.0f) {
			glm::vec3 ndc = glm::vec3(clip) / clip.w;

			// check it's in screen range
			if (ndc.x >= -1.0f && ndc.x <= 1.0f &&
				ndc.y >= -1.0f && ndc.y <= 1.0f) {
					enemy_visible = true;

				// depth sampling logic stays the same, just using c_world / execution_target
				// (copy your existing enemy-depth code and replace `enemy` with `execution_target`)
				// ...
				// enemy_visible = !(depth_sample + eps < enemy_depth);
			}
		}
	}
	
	// draw aim indicator / outline on target civilian in focus mode
	if (focus_mode && execution_target && enemy_visible) {
		glm::mat4 clip_from_world =
			camera->make_projection() * glm::mat4(camera->transform->make_local_from_world());

		glm::mat4x3 world_from_target = execution_target->make_world_from_local();
		glm::vec3 c_world = world_from_target[3];           // translation column
		glm::vec4 c_clip  = clip_from_world * glm::vec4(c_world, 1.0f);

		if (c_clip.w > 0.0f) {
			// glm::vec3 c_ndc = glm::vec3(c_clip) / c_clip.w;
			// ... existing draw_lines / crosshair overlay code, just driven by c_ndc instead of enemy
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
	//11/23 update Game over logic 
	/*
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
	*/
	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f));

		constexpr float H = 0.05f;
		// 11/24 update Alex Ding
		// --- Kill counter UI: top-right, only while PLAYING ---
		if (screen_state == ScreenState::PLAYING) {
			std::string kill_text = std::to_string(kill_count) + "/5";

			glm::u8vec4 kc_color = glm::u8vec4(0xff, 0xff, 0xff, 0xff);

			// position near top-right of the screen:
			float x = aspect - 4.0f * H;   // a bit from right edge
			float y = 1.0f - 2.0f * H;     // a bit down from top

			lines.draw_text(
				kill_text,
				glm::vec3(x, y, 0.0f),
				glm::vec3(H*2.0f, 0.0f, 0.0f),   // x-step
				glm::vec3(0.0f, H*2.0f, 0.0f),   // y-step
				kc_color
			);
		}
		// --- Dash Tutor Word showed up ---
		//11/24 update Alex Ding
		if (dash_hint_active) {
			glm::u8vec4 hint_color = glm::u8vec4(255, 255, 255, 255);

			// ---- Center of screen ----
			glm::vec3 anchor = glm::vec3(-0.3f, 0.0f, 0.0f);

			// ---- BIG text scale ----
			// You can increase 0.2f → 0.25f or 0.3f if you want it even bigger
			float pulse     = 0.5f + 0.5f * std::cos(dash_hint_timer * 4.0f);
			float size      = 0.1f * (0.8f + 0.4f * pulse);
			glm::vec3 x_dir = glm::vec3(size, 0.0f, 0.0f);   // width
			glm::vec3 y_dir = glm::vec3(0.0f, size, 0.0f);   // height

			lines.draw_text(
				"PRESS SPACE TO DASH",
				anchor,
				x_dir,
				y_dir,
				hint_color,
				nullptr
				); // color
		}

		if (sound_hint_active) {
			glm::u8vec4 hint_color = glm::u8vec4(255, 255, 255, 255);

			// ---- Center of screen ----
			glm::vec3 anchor = glm::vec3(-0.4f, 0.0f, 0.0f);

			// ---- BIG text scale ----
			// You can increase 0.2f → 0.25f or 0.3f if you want it even bigger

			float pulse     = 0.5f + 0.5f * std::cos(sound_hint_timer * 4.0f);
			float size      = 0.1f * (0.8f + 0.4f * pulse);
			glm::vec3 x_dir = glm::vec3(size, 0.0f, 0.0f);   // width
			glm::vec3 y_dir = glm::vec3(0.0f, size, 0.0f);   // height

			lines.draw_text(
				"PRESS G TO Make Attraction",
				anchor,
				x_dir,
				y_dir,
				hint_color,
				nullptr
				); // color
		}

		if (pass_hint_active) {
			glm::u8vec4 hint_color = glm::u8vec4(255, 255, 255, 255);

			// ---- Center of screen ----
			glm::vec3 anchor = glm::vec3(-0.3f, 0.0f, 0.0f);

			// ---- BIG text scale ----
			// You can increase 0.2f → 0.25f or 0.3f if you want it even bigger
			float pulse     = 0.5f + 0.5f * std::cos(pass_hint_timer * 4.0f);
			float size      = 0.1f * (0.8f + 0.4f * pulse);
			glm::vec3 x_dir = glm::vec3(size, 0.0f, 0.0f);   // width
			glm::vec3 y_dir = glm::vec3(0.0f, size, 0.0f);   // height

			lines.draw_text(
				"Gate is opened! Time to Run Away!",
				anchor,
				x_dir,
				y_dir,
				hint_color,
				nullptr
				); // color
		}
	
		if (game_success)
		{
			lines.draw_text("You escaped the zoo... You are free!",
							glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
							glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
							glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text("You escaped the zoo... You are free!",
							glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + +0.1f * H + ofs, 0.0),
							glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
							glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else
		{
			lines.draw_text("WASD moves character. Right click to stalk the human visitor to learn how human walks. Left click to attack when you have finished learning...",
							glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
							glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
							glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text("WASD moves character. Right click to stalk the human visitor to learn how human walks. Left click to attack when you have finished learning...",
							glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + +0.1f * H + ofs, 0.0),
							glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
							glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
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