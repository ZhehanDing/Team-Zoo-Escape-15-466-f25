// Credit: Adapted from 15-466-f19-base6, Professor Tim's lighting demo code.

#include "BasicMaterialDeferredInstancingProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Scene::Drawable::Pipeline basic_material_deferred_object_instancing_program_pipeline;

// Creates a global program object that loads early in initialization
Load< BasicMaterialDeferredObjectInstancingProgram > basic_material_deferred_object_instancing_program(LoadTagEarly, []() -> BasicMaterialDeferredObjectInstancingProgram const * {
    // Create a new instance of the deferred rendering program
    BasicMaterialDeferredObjectInstancingProgram *ret = new BasicMaterialDeferredObjectInstancingProgram();

    //----- build the pipeline template -----
    // Link the shader program to the pipeline
    basic_material_deferred_object_instancing_program_pipeline.program = ret->program;

    // Set up transformation matrices in the pipeline:
    // Matrix for transforming object space to clip space (projection * view * model)
    // basic_material_deferred_object_instancing_program_pipeline.CLIP_FROM_OBJECT_mat4 = ret->OBJECT_TO_CLIP_mat4;
    // Matrix for transforming object space to light space (3D positions)
    // basic_material_deferred_object_instancing_program_pipeline.LIGHT_FROM_OBJECT_mat4x3 = ret->OBJECT_TO_LIGHT_mat4x3;
    // Matrix for transforming normals to light space
    // basic_material_deferred_object_instancing_program_pipeline.LIGHT_FROM_NORMAL_mat3 = ret->NORMAL_TO_LIGHT_mat3;

    //make a 1-pixel white texture to bind by default:
    // Create texture handle
    GLuint tex;
    glGenTextures(1, &tex);

    // Set up a default white texture:
    glBindTexture(GL_TEXTURE_2D, tex);
    // Create a single white pixel (0xff = 255 for all components)
    std::vector< glm::u8vec4 > tex_data(1, glm::u8vec4(0xff));
    // Upload the white pixel to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data.data());
    // Set texture wrapping mode to clamp at edges
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Set texture filtering to nearest neighbor (no interpolation needed for 1 pixel)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);

    // Assign the white texture to the first texture slot in the pipeline
    basic_material_deferred_object_instancing_program_pipeline.textures[0].texture = tex;
    // Set the texture target type to 2D
    basic_material_deferred_object_instancing_program_pipeline.textures[0].target = GL_TEXTURE_2D;

    // Return the created program
    return ret;
});

BasicMaterialDeferredObjectInstancingProgram::BasicMaterialDeferredObjectInstancingProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"                            // Using GLSL version 3.30
		"#line " STR(__LINE__) "\n"
		"uniform mat4 WORLD_TO_CLIP;\n"
		"uniform mat4x3 WORLD_TO_LIGHT;\n"
		"in vec4 Position;\n"                       // Input vertex position
		"in vec3 Normal;\n"                         // Input vertex normal
		"in vec4 Color;\n"                          // Input vertex color
		"in vec2 TexCoord;\n"                       // Input texture coordinates
		"layout(location=7) in mat4 WorldFromLocal;\n"
		"out vec3 position;\n"                      // Output position in light space
		"out vec3 normal;\n"                        // Output normal in light space
		"out vec4 color;\n"                         // Output vertex color
		"out vec2 texCoord;\n"                      // Output texture coordinates
		"void main() {\n"
		"	vec4 world_pos = WorldFromLocal * Position;\n"    // Transform vertex to clip space
		"	gl_Position = WORLD_TO_CLIP * world_pos;\n"
		"	position = WORLD_TO_LIGHT * world_pos;\n"       // Transform vertex to light space
		"	normal = inverse(transpose(mat3(WORLD_TO_LIGHT * WorldFromLocal))) * Normal;\n"           // Transform normal to light space
		"	color = Color;\n"                               // Pass through vertex color
		"	texCoord = TexCoord;\n"      
		"}\n"
	,
		//fragment shader:
		"#version 330\n"                            // Using GLSL version 3.30
		"#line " STR(__LINE__) "\n"
		"uniform sampler2D TEX;\n"                  // Input texture sampler
		"uniform float ROUGHNESS;\n"                // Material roughness parameter
		"in vec3 position;\n"                       // Input interpolated position
		"in vec3 normal;\n"                         // Input interpolated normal
		"in vec4 color;\n"                          // Input interpolated color
		"in vec2 texCoord;\n"                       // Input interpolated texture coordinates
		"layout(location=0) out vec4 fragPosition;\n"        // Output 1: Position G-buffer
		"layout(location=1) out vec4 fragNormalRoughness;\n" // Output 2: Normal+Roughness G-buffer
		"layout(location=2) out vec4 fragAlbedo;\n"          // Output 3: Albedo G-buffer
		"void main() {\n"
		"	fragPosition = vec4(position, 0.0);\n"              // Store position in G-buffer
		"	fragNormalRoughness = vec4(normalize(normal), ROUGHNESS);\n"  // Store normalized normal and roughness
		"	fragAlbedo = texture(TEX, texCoord) * color;\n"     // Store texture color modulated by vertex color
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	Normal_vec3 = glGetAttribLocation(program, "Normal");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");
	World_From_Local_mat4 = glGetAttribLocation(program, "WorldFromLocal");

	//look up the locations of uniforms:

	ROUGHNESS_float = glGetUniformLocation(program, "ROUGHNESS");

	WORLD_TO_CLIP_mat4 = glGetUniformLocation(program, "WORLD_TO_CLIP");
	WORLD_TO_LIGHT_mat4x3 = glGetUniformLocation(program, "WORLD_TO_LIGHT");

	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	SKY_MODE_int = glGetUniformLocation(program, "SKY_MODE");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

BasicMaterialDeferredObjectInstancingProgram::~BasicMaterialDeferredObjectInstancingProgram() {
	glDeleteProgram(program);
	program = 0;
}