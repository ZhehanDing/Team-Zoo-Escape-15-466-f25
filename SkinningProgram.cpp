#include "SkinningProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

#define BONE_LIMIT 128

Scene::Drawable::Pipeline skinning_program_pipeline;

Load< SkinningProgram > skinning_program(LoadTagEarly, []() -> SkinningProgram const * {
	SkinningProgram *ret = new SkinningProgram();

	//----- build the pipeline template -----
	skinning_program_pipeline.program = ret->program;

	skinning_program_pipeline.CLIP_FROM_OBJECT_mat4 = ret->CLIP_FROM_OBJECT_mat4;
	skinning_program_pipeline.LIGHT_FROM_OBJECT_mat4x3 = ret->LIGHT_FROM_OBJECT_mat4x3;
	skinning_program_pipeline.LIGHT_FROM_NORMAL_mat3 = ret->LIGHT_FROM_NORMAL_mat3;

	/*
	// This will be used later if/when we build a light loop into the Scene:
	skinning_program_pipeline.LIGHT_TYPE_int = ret->LIGHT_TYPE_int;
	skinning_program_pipeline.LIGHT_LOCATION_vec3 = ret->LIGHT_LOCATION_vec3;
	skinning_program_pipeline.LIGHT_DIRECTION_vec3 = ret->LIGHT_DIRECTION_vec3;
	skinning_program_pipeline.LIGHT_ENERGY_vec3 = ret->LIGHT_ENERGY_vec3;
	skinning_program_pipeline.LIGHT_CUTOFF_float = ret->LIGHT_CUTOFF_float;
	*/
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


	skinning_program_pipeline.textures[0].texture = tex;
	skinning_program_pipeline.textures[0].target = GL_TEXTURE_2D;

	return ret;
});

SkinningProgram::SkinningProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 CLIP_FROM_OBJECT;\n"
		"uniform mat4x3 LIGHT_FROM_OBJECT;\n"
		"uniform mat3 LIGHT_FROM_NORMAL;\n"
		"uniform mat4x3 POSE[" + std::to_string(BONE_LIMIT) + "];\n"
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
		"   vec3 skinned_position = (\n"
		"   	BoneWeights.x * POSE[ BoneIndices.x ] +\n"
		"   	BoneWeights.y * POSE[ BoneIndices.y ] +\n"
		"   	BoneWeights.z * POSE[ BoneIndices.z ] +\n"
		"   	BoneWeights.w * POSE[ BoneIndices.w ]) * Position;\n"
		"   vec3 skinned_normal = (\n"
		"   	BoneWeights.x * mat3(POSE[ BoneIndices.x ]) +\n"
		"   	BoneWeights.y * mat3(POSE[ BoneIndices.y ]) +\n"
		"   	BoneWeights.z * mat3(POSE[ BoneIndices.z ]) +\n"
		"   	BoneWeights.w * mat3(POSE[ BoneIndices.w ])) * Normal;\n" // maybe push inverse pose info to compute normal
		"	gl_Position = CLIP_FROM_OBJECT * vec4(skinned_position, 1.0);\n"
		"	position = LIGHT_FROM_OBJECT * vec4(skinned_position, 1.0);\n"
		"	normal = LIGHT_FROM_NORMAL * skinned_normal;\n"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TEX;\n"
		"uniform int LIGHT_TYPE;\n"
		"uniform vec3 LIGHT_LOCATION;\n"
		"uniform vec3 LIGHT_DIRECTION;\n"
		"uniform vec3 LIGHT_ENERGY;\n"
		"uniform float LIGHT_CUTOFF;\n"
		"in vec3 position;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"out vec4 fragColor;\n"
		"float random(vec2 st) { //from https://thebookofshaders.com/10/\n"
		"	return fract(sin(dot(st, vec2(12.9898, 78.233)))*43758.5453123);\n"
		"}\n"
		"void main() {\n"
		"	vec3 n = normalize(normal);\n"
		"	vec3 e;\n"
		"	if (LIGHT_TYPE == 0) { //point light \n"
		"		vec3 l = (LIGHT_LOCATION - position);\n"
		"		float dis2 = dot(l,l);\n"
		"		l = normalize(l);\n"
		"		float nl = max(0.0, dot(n, l)) / max(1.0, dis2);\n"
		"		e = nl * LIGHT_ENERGY;\n"
		"	} else if (LIGHT_TYPE == 1) { //hemi light \n"
		"		e = (dot(n,-LIGHT_DIRECTION) * 0.5 + 0.5) * LIGHT_ENERGY;\n"
		"	} else if (LIGHT_TYPE == 2) { //spot light \n"
		"		vec3 l = (LIGHT_LOCATION - position);\n"
		"		float dis2 = dot(l,l);\n"
		"		l = normalize(l);\n"
		"		float nl = max(0.0, dot(n, l)) / max(1.0, dis2);\n"
		"		float c = dot(l,-LIGHT_DIRECTION);\n"
		"		nl *= smoothstep(LIGHT_CUTOFF,mix(LIGHT_CUTOFF,1.0,0.1), c);\n"
		"		e = nl * LIGHT_ENERGY;\n"
		"	} else { //(LIGHT_TYPE == 3) //directional light \n"
		"		e = max(0.0, dot(n,-LIGHT_DIRECTION)) * LIGHT_ENERGY;\n"
		"	}\n"
		"	vec4 albedo = color;\n"
		"	fragColor = vec4(albedo.rgb, albedo.a);\n"
		/* DEBUG: check color output linearity:
		"	float t = random(gl_FragCoord.xy/1280.0);\n"
		"	float amt = fract(gl_FragCoord.x/512.0);\n"
		"	if (fract(gl_FragCoord.y / 128.0) > 0.5) {\n"
		"		if (amt > t) {\n"
		"			fragColor = vec4(1.0,1.0,1.0,1.0);\n"
		"		} else {\n"
		"			fragColor = vec4(0.0,0.0,0.0,1.0);\n"
		"		}\n"
		"	} else {\n"
		"		fragColor = vec4(amt,amt,amt,1.0);\n"
		"	}\n"
		*/
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

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

	BONE_POSE_mat4x3 = glGetUniformLocation(program, "POSE");

	LIGHT_TYPE_int = glGetUniformLocation(program, "LIGHT_TYPE");
	LIGHT_LOCATION_vec3 = glGetUniformLocation(program, "LIGHT_LOCATION");
	LIGHT_DIRECTION_vec3 = glGetUniformLocation(program, "LIGHT_DIRECTION");
	LIGHT_ENERGY_vec3 = glGetUniformLocation(program, "LIGHT_ENERGY");
	LIGHT_CUTOFF_float = glGetUniformLocation(program, "LIGHT_CUTOFF");


	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

SkinningProgram::~SkinningProgram() {
	glDeleteProgram(program);
	program = 0;
}

