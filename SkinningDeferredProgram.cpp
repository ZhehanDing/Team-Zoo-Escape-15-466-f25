#include "SkinningDeferredProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

#define BONE_LIMIT 256

Scene::Drawable::Pipeline skinning_deferred_program_pipeline;

Load< SkinningDeferredProgram > skinning_deferred_program(LoadTagEarly, []() -> SkinningDeferredProgram const * {
	SkinningDeferredProgram *ret = new SkinningDeferredProgram();
		
    //----- build the pipeline template -----
    skinning_deferred_program_pipeline.program = ret->program;

	skinning_deferred_program_pipeline.CLIP_FROM_OBJECT_mat4 = ret->CLIP_FROM_OBJECT_mat4;
	skinning_deferred_program_pipeline.LIGHT_FROM_OBJECT_mat4x3 = ret->LIGHT_FROM_OBJECT_mat4x3;
	skinning_deferred_program_pipeline.LIGHT_FROM_NORMAL_mat3 = ret->LIGHT_FROM_NORMAL_mat3;

    //make a 1-pixel white texture to bind by default:
	GLuint tex;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	std::vector< glm::u8vec4 > tex_data(1, glm::u8vec4(0xff));
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	skinning_deferred_program_pipeline.textures[0].texture = tex;
	skinning_deferred_program_pipeline.textures[0].target = GL_TEXTURE_2D;

	return ret;
});

SkinningDeferredProgram::SkinningDeferredProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 CLIP_FROM_OBJECT;\n"
		"uniform mat4x3 LIGHT_FROM_OBJECT;\n"
		"uniform mat3 LIGHT_FROM_NORMAL;\n"
		"layout(std140) uniform PoseBlock { mat4 POSE[" + std::to_string(BONE_LIMIT) + "]; };\n"
		"in vec4 Position;\n"
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"in vec2 TexCoord;\n"
		"in uvec4 BoneIndices;\n"
		"in vec4 BoneWeights;\n"
		"out vec3 position;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
		"   vec4 skinned_pos4 = (\n"
		"   \tBoneWeights.x * POSE[ BoneIndices.x ] +\n"
		"   \tBoneWeights.y * POSE[ BoneIndices.y ] +\n"
		"   \tBoneWeights.z * POSE[ BoneIndices.z ] +\n"
		"   \tBoneWeights.w * POSE[ BoneIndices.w ]) * Position;\n"
		"   vec3 skinned_normal = (\n"
		"   \tBoneWeights.x * mat3(POSE[ BoneIndices.x ]) +\n"
		"   \tBoneWeights.y * mat3(POSE[ BoneIndices.y ]) +\n"
		"   \tBoneWeights.z * mat3(POSE[ BoneIndices.z ]) +\n"
		"   \tBoneWeights.w * mat3(POSE[ BoneIndices.w ])) * Normal;\n"
		"	gl_Position = CLIP_FROM_OBJECT * skinned_pos4;\n"
		"	position = LIGHT_FROM_OBJECT * skinned_pos4;\n"
		"	normal = LIGHT_FROM_NORMAL * skinned_normal;\n"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TEX;\n"
		"uniform float ROUGHNESS;\n"
		"in vec3 position;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"layout(location=0) out vec4 fragPosition;\n"
		"layout(location=1) out vec4 fragNormalRoughness;\n"
		"layout(location=2) out vec4 fragAlbedo;\n"
		"void main() {\n"
		"	fragPosition = vec4(position, 0.0);\n"
		"	fragNormalRoughness = vec4(normalize(normal), ROUGHNESS);\n"
		"	fragAlbedo = texture(TEX, texCoord) * color;\n"
		"}\n"
	);

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	Normal_vec3 = glGetAttribLocation(program, "Normal");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");
	Bone_Indices_uvec4 = glGetAttribLocation(program, "BoneIndices");
	Bone_Weights_vec4 = glGetAttribLocation(program, "BoneWeights");

	//look up the locations of uniforms:
	CLIP_FROM_OBJECT_mat4 = glGetUniformLocation(program, "CLIP_FROM_OBJECT");
	LIGHT_FROM_OBJECT_mat4x3 = glGetUniformLocation(program, "LIGHT_FROM_OBJECT");
	LIGHT_FROM_NORMAL_mat3 = glGetUniformLocation(program, "LIGHT_FROM_NORMAL");

	ROUGHNESS_float = glGetUniformLocation(program, "ROUGHNESS");

	// Bind the PoseBlock uniform block (std140, binding = 3)
	GLuint pose_block = glGetUniformBlockIndex(program, "PoseBlock");
	if (pose_block != GL_INVALID_INDEX) glUniformBlockBinding(program, pose_block, 3);

	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

SkinningDeferredProgram::~SkinningDeferredProgram() {
	glDeleteProgram(program);
	program = 0;
}
