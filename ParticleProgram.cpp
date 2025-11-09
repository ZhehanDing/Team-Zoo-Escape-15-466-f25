#include "ParticleProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

//modified lit color texture program
Scene::Drawable::Pipeline particle_program_pipeline;

Load< ParticleProgram > particle_program(LoadTagEarly, []() -> ParticleProgram const * {
	ParticleProgram *ret = new ParticleProgram();

	//----- build the pipeline template -----
	particle_program_pipeline.program = ret->program;

	particle_program_pipeline.CLIP_FROM_OBJECT_mat4 = ret->CLIP_FROM_OBJECT_mat4;
	particle_program_pipeline.LIGHT_FROM_OBJECT_mat4x3 = ret->LIGHT_FROM_OBJECT_mat4x3;
	particle_program_pipeline.LIGHT_FROM_NORMAL_mat3 = ret->LIGHT_FROM_NORMAL_mat3;

	/* This will be used later if/when we build a light loop into the Scene:
	particle_program_pipeline.LIGHT_TYPE_int = ret->LIGHT_TYPE_int;
	particle_program_pipeline.LIGHT_LOCATION_vec3 = ret->LIGHT_LOCATION_vec3;
	particle_program_pipeline.LIGHT_DIRECTION_vec3 = ret->LIGHT_DIRECTION_vec3;
	particle_program_pipeline.LIGHT_ENERGY_vec3 = ret->LIGHT_ENERGY_vec3;
	particle_program_pipeline.LIGHT_CUTOFF_float = ret->LIGHT_CUTOFF_float;
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


	particle_program_pipeline.textures[0].texture = tex;
	particle_program_pipeline.textures[0].target = GL_TEXTURE_2D;

	return ret;
});

ParticleProgram::ParticleProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 CLIP_FROM_OBJECT;\n"
		"uniform mat4x3 LIGHT_FROM_OBJECT;\n"
		"uniform mat3 LIGHT_FROM_NORMAL;\n"
		"uniform float ASPECT;\n"
		"in vec3 Position;\n"
		"in float Size;\n"
		"in vec2 Corner;\n"
		"out vec3 position;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
		"	vec4 clip = CLIP_FROM_OBJECT * vec4(Position, 1.0);\n"
		"	vec3 ndc = clip.xyz / clip.w;\n"
		// up to constants - https://stackoverflow.com/questions/42751427/transformations-from-pixels-to-ndc
		"	gl_Position = clip + vec4(Corner.x * Size / ASPECT, Corner.y * Size, 0.0, 0.0);\n"
		"	color = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"	texCoord = Corner + vec2(0.5);\n"
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
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"out vec4 fragColor;\n"
		"float random(vec2 st) { //from https://thebookofshaders.com/10/\n"
		"	return fract(sin(dot(st, vec2(12.9898, 78.233)))*43758.5453123);\n"
		"}\n"
		"void main() {\n"
		"	vec4 albedo = texture(TEX, texCoord) * color;\n"
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
	Size_float = glGetAttribLocation(program, "Size");
	Corner_vec2 = glGetAttribLocation(program, "Corner");

	//look up the locations of uniforms:
	CLIP_FROM_OBJECT_mat4 = glGetUniformLocation(program, "CLIP_FROM_OBJECT");
	LIGHT_FROM_OBJECT_mat4x3 = glGetUniformLocation(program, "LIGHT_FROM_OBJECT");
	LIGHT_FROM_NORMAL_mat3 = glGetUniformLocation(program, "LIGHT_FROM_NORMAL");
	ASPECT = glGetUniformLocation(program, "ASPECT");

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

ParticleProgram::~ParticleProgram() {
	glDeleteProgram(program);
	program = 0;
}

