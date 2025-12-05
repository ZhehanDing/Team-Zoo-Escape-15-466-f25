#include "MenuScene.hpp"

#include "data_path.hpp"

#include "Textures.hpp"
#include "LightMeshes.hpp"

#include "BasicMaterialDeferredProgram.hpp"


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