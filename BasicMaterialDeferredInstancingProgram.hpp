// Credit: Adapted from 15-466-f19-base6, Professor Tim's lighting demo code.

#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

//Shader program that draws transformed, lit, textured vertices tinted with vertex colors:
//This is the first half -- it stores position, [normal,roughness], albedo to framebuffers:
struct BasicMaterialDeferredObjectInstancingProgram {
	BasicMaterialDeferredObjectInstancingProgram();
	~BasicMaterialDeferredObjectInstancingProgram();

	GLuint program = 0;

	//Attribute (per-vertex variable) locations:
	GLuint Position_vec4 = -1U;
	GLuint Normal_vec3 = -1U;
	GLuint Color_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;
	GLuint World_From_Local_mat4 = -1U;

	//Uniform (per-invocation variable) locations:
	GLuint WORLD_TO_CLIP_mat4 = -1U;
	GLuint WORLD_TO_LIGHT_mat4x3 = -1U;

	//  material uniforms:
	GLuint ROUGHNESS_float = -1U;

	GLint SKY_MODE_int = -1; // 0 = normal object, 1 = sky dome

	//Textures:
	//TEXTURE0 - texture that is accessed by TexCoord

	//Framebuffers:
	//0 - position
	//1 - (normal, roughness)
	//2 - albedo
};

extern Load< BasicMaterialDeferredObjectInstancingProgram > basic_material_deferred_object_instancing_program;

//For convenient scene-graph setup, copy this object:
// NOTE: by default, has texture bound to 1-pixel white texture -- so it's okay to use with vertex-color-only meshes.
extern Scene::Drawable::Pipeline basic_material_deferred_object_instancing_program_pipeline;