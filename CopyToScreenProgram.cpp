#include "CopyToScreenProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

GLuint empty_vao = 0;

Load<void> init_empty_vao(LoadTagEarly, []()
						  {
							  glGenVertexArrays(1, &empty_vao); // Creates a Vertex Array Object that contains no vertex attribute data
						  });

Load<CopyToScreenProgram> copy_to_screen_program(LoadTagEarly);

CopyToScreenProgram::CopyToScreenProgram()
{
	// Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		// vertex shader:
		"#version 330\n" // Specify GLSL version 3.30
		"void main() {\n"
		"	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n" // Calculate vertex positions for a fullscreen quad using vertex ID
		"}\n",
		// fragment shader:
		"#version 330\n"		   // Specify GLSL version 3.30
		"uniform sampler2D TEX;\n" // Declare texture sampler uniform
		"out vec4 fragColor;\n"	   // Declare output color
		"void main() {\n"
		"	fragColor = texelFetch(TEX, ivec2(gl_FragCoord), 0);\n" // Copy texture pixel to screen at current fragment coordinate
		"}\n");

	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	// set TEX to always refer to texture binding zero:
	glUseProgram(program);		   // bind program -- glUniform* calls refer to this program now
	glUniform1i(TEX_sampler2D, 0); // set TEX to sample from GL_TEXTURE0
	glUseProgram(0);			   // unbind program -- glUniform* calls refer to ??? now
}

CopyToScreenProgram::~CopyToScreenProgram()
{
	glDeleteProgram(program);
	program = 0;
}
