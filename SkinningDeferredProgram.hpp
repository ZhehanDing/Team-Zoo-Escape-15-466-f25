#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct SkinningDeferredProgram {
	SkinningDeferredProgram();
	~SkinningDeferredProgram();

	GLuint program = 0;

	//Attribute (per-vertex variable) locations:
	GLuint Position_vec4 = -1U;
	GLuint Normal_vec3 = -1U;
	GLuint Color_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;
	GLuint Bone_Indices_uvec4 = -1U;
	GLuint Bone_Weights_vec4 = -1U;

	//Uniform (per-invocation variable) locations:
	GLuint CLIP_FROM_OBJECT_mat4 = -1U;
	GLuint LIGHT_FROM_OBJECT_mat4x3 = -1U;
	GLuint LIGHT_FROM_NORMAL_mat3 = -1U;

	//  material uniforms:
	GLuint ROUGHNESS_float = -1U;

	//Textures:
	//TEXTURE0 - texture that is accessed by TexCoord
};

extern Load< SkinningDeferredProgram > skinning_deferred_program;

//For convenient scene-graph setup, copy this object:
// NOTE: by default, has texture bound to 1-pixel white texture -- so it's okay to use with vertex-color-only meshes.
extern Scene::Drawable::Pipeline skinning_deferred_program_pipeline;
