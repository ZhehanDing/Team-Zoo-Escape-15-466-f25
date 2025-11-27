#include "LightStencilProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< LightStencilProgram > light_stencil_program(LoadTagEarly);

LightStencilProgram::LightStencilProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"in vec4 Position;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"void main() { }"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
}

LightStencilProgram::~LightStencilProgram() {
	glDeleteProgram(program);
	program = 0;
}

