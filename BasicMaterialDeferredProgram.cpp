// Credit: Adapted from 15-466-f19-base6, Professor Tim's lighting demo code.

#include "BasicMaterialDeferredProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Scene::Drawable::Pipeline basic_material_deferred_object_program_pipeline;

// Creates a global program object that loads early in initialization
Load< BasicMaterialDeferredObjectProgram > basic_material_deferred_object_program(LoadTagEarly, []() -> BasicMaterialDeferredObjectProgram const * {
    // Create a new instance of the deferred rendering program
    BasicMaterialDeferredObjectProgram *ret = new BasicMaterialDeferredObjectProgram();

    //----- build the pipeline template -----
    // Link the shader program to the pipeline
    basic_material_deferred_object_program_pipeline.program = ret->program;

    // Set up transformation matrices in the pipeline:
    // Matrix for transforming object space to clip space (projection * view * model)
    basic_material_deferred_object_program_pipeline.CLIP_FROM_OBJECT_mat4 = ret->OBJECT_TO_CLIP_mat4;
    // Matrix for transforming object space to light space (3D positions)
    basic_material_deferred_object_program_pipeline.LIGHT_FROM_OBJECT_mat4x3 = ret->OBJECT_TO_LIGHT_mat4x3;
    // Matrix for transforming normals to light space
    basic_material_deferred_object_program_pipeline.LIGHT_FROM_NORMAL_mat3 = ret->NORMAL_TO_LIGHT_mat3;

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
    basic_material_deferred_object_program_pipeline.textures[0].texture = tex;
    // Set the texture target type to 2D
    basic_material_deferred_object_program_pipeline.textures[0].target = GL_TEXTURE_2D;

    // Return the created program
    return ret;
});

BasicMaterialDeferredObjectProgram::BasicMaterialDeferredObjectProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"                            // Using GLSL version 3.30
		"#line " STR(__LINE__) "\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"            // Transform matrix from object space to clip space
		"uniform mat4x3 OBJECT_TO_LIGHT;\n"         // Transform matrix from object space to light space (3x4 matrix)
		"uniform mat3 NORMAL_TO_LIGHT;\n"           // Transform matrix for normals to light space
		"uniform int SKY_MODE;\n"          			// 0 = normal object, 1 = sky dome
		"in vec4 Position;\n"                       // Input vertex position
		"in vec3 Normal;\n"                         // Input vertex normal
		"in vec4 Color;\n"                          // Input vertex color
		"in vec2 TexCoord;\n"                       // Input texture coordinates
		"out vec3 position;\n"                      // Output position in light space
		"out vec3 normal;\n"                        // Output normal in light space
		"out vec4 color;\n"                         // Output vertex color
		"out vec2 texCoord;\n"                      // Output texture coordinates
		"void main() {\n"
		"	vec4 pos = Position;\n"
		"    if (SKY_MODE != 0) {\n"
		"        pos.w = 0.0;\n"					// treat as direction, not point
		"    }\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"    // Transform vertex to clip space
		"	position = OBJECT_TO_LIGHT * Position;\n"       // Transform vertex to light space
		"	normal = NORMAL_TO_LIGHT * Normal;\n"           // Transform normal to light space
		"	color = Color;\n"                               // Pass through vertex color
		"	texCoord = TexCoord;\n"                        // Pass through texture coordinates
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

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	OBJECT_TO_LIGHT_mat4x3 = glGetUniformLocation(program, "OBJECT_TO_LIGHT");
	NORMAL_TO_LIGHT_mat3 = glGetUniformLocation(program, "NORMAL_TO_LIGHT");

	ROUGHNESS_float = glGetUniformLocation(program, "ROUGHNESS");

	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	SKY_MODE_int = glGetUniformLocation(program, "SKY_MODE");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

BasicMaterialDeferredObjectProgram::~BasicMaterialDeferredObjectProgram() {
	glDeleteProgram(program);
	program = 0;
}


//-----------------------------------------------


Scene::Drawable::Pipeline basic_material_deferred_light_program_pipeline;

Load< BasicMaterialDeferredLightProgram > basic_material_deferred_light_program(LoadTagEarly, []() -> BasicMaterialDeferredLightProgram const * {
	BasicMaterialDeferredLightProgram *ret = new BasicMaterialDeferredLightProgram();

	//----- build the pipeline template -----
	basic_material_deferred_light_program_pipeline.program = ret->program;

	basic_material_deferred_light_program_pipeline.CLIP_FROM_OBJECT_mat4 = ret->OBJECT_TO_CLIP_mat4;

	return ret;
});


BasicMaterialDeferredLightProgram::BasicMaterialDeferredLightProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		// -- vertex shader --
		"#version 330\n"                              // Using GLSL version 330 (OpenGL 3.3)
		"uniform mat4 OBJECT_TO_CLIP;\n"              // Transform matrix from object space to clip space
		"in vec4 Position;\n"                         // Input vertex position
		"void main() {\n"
		"   gl_Position = OBJECT_TO_CLIP * Position;\n" // Transform vertex to clip space
		"}\n"
	,
		// -- fragment shader --
		"#version 330\n"
		// G-buffer textures containing geometric and material information
		"uniform sampler2D POSITION_TEX;\n"           // World-space position
		"uniform sampler2D NORMAL_ROUGHNESS_TEX;\n"   // Surface normal and roughness
		"uniform sampler2D ALBEDO_TEX;\n"            // Surface color/texture

		// Light properties
		"uniform int LIGHT_TYPE;\n"                   // 0=point, 1=hemi, 2=spot, 3=directional
		"uniform vec3 LIGHT_LOCATION;\n"              // Position of light source
		"uniform vec3 LIGHT_DIRECTION;\n"             // Direction of light (for directional/spot lights)
		"uniform vec3 LIGHT_ENERGY;\n"                // Light color and intensity
		"uniform float LIGHT_CUTOFF;\n"               // Cosine of spotlight cutoff angle
		"uniform vec3 EYE;\n"                         // Camera position in world space
		"out vec4 fragColor;\n"                       // Final output color

		"void main() {\n"
			// Fetch G-buffer data at current pixel
			"vec3 position = texelFetch(POSITION_TEX, ivec2(gl_FragCoord.xy), 0).xyz;\n"          // Get world position
			"vec4 normal_roughness = texelFetch(NORMAL_ROUGHNESS_TEX, ivec2(gl_FragCoord.xy), 0);\n" // Get normal and roughness
			"vec4 albedo = texelFetch(ALBEDO_TEX, ivec2(gl_FragCoord.xy), 0);\n"                 // Get surface color
			
			// Calculate material properties
			"float shininess = pow(1024.0, 1.0 - normal_roughness.w);\n"     // Convert roughness to shininess
			"vec3 n = normalize(normal_roughness.xyz);\n"                     // Normalized surface normal
			"vec3 v = normalize(EYE - position);\n"                          // View direction
			
			// Vectors used in lighting calculation
			"vec3 l; //direction to light\n"  // Direction to light source
			"vec3 h; //half-vector\n" // Half vector between view and light
			"vec3 e; //light flux\n" // Light energy reaching surface

			// Point light calculation
			"if (LIGHT_TYPE == 0) {\n"
				"l = (LIGHT_LOCATION - position);\n"              // Vector to light
				"float dis2 = dot(l,l);\n"                       // Square distance for attenuation
				"l = normalize(l);\n"
				"h = normalize(l+v);\n"                          // Half vector for specular
				"float nl = max(0.0, dot(n, l)) / max(1.0, dis2);\n" // Lambert's law with distance attenuation
				"e = nl * LIGHT_ENERGY;\n"
			
			// Hemispheric light calculation
			"} else if (LIGHT_TYPE == 1) {\n"
				"l = -LIGHT_DIRECTION;\n"
				"h = vec3(0.0);\n"                              // No specular for hemisphere lighting for now
				"e = (dot(n,l) * 0.5 + 0.5) * LIGHT_ENERGY;\n" // Interpolate between sky and ground colors
			
			// Spotlight calculation
			"} else if (LIGHT_TYPE == 2) {\n"
				"l = (LIGHT_LOCATION - position);\n"
				"float dis2 = dot(l,l);\n"
				"l = normalize(l);\n"
				"h = normalize(l+v);\n"
				"float nl = max(0.0, dot(n, l)) / max(1.0, dis2);\n"
				"float c = dot(l,-LIGHT_DIRECTION);\n"           // Cosine of angle to spotlight direction
				"nl *= smoothstep(LIGHT_CUTOFF,mix(LIGHT_CUTOFF,1.0,0.1), c);\n" // Smooth spotlight edge
				"e = nl * LIGHT_ENERGY;\n"
			
			// Directional light calculation
			"} else {\n"
				"l = -LIGHT_DIRECTION;\n"
				"h = normalize(l+v);\n"
				"e = max(0.0, dot(n,l)) * LIGHT_ENERGY;\n"      // Simple directional light
			"}\n"

			// Calculate final surface reflectance
			"vec3 reflectance =\n"
				"albedo.rgb / 3.1415926\n"                      // Lambertian diffuse term
				"+ pow(max(0.0, dot(n, h)), shininess)\n"       // Blinn-Phong specular term
				"  * (shininess + 2.0) / (8.0)\n"              // Energy conservation normalization
				"  * mix(0.04, 1.0, pow(1.0 - max(0.0, dot(h, v)), 5.0))\n" // Fresnel effect
			";\n"
			
			// Final color combining light and surface properties
			"fragColor = vec4(e*reflectance, albedo.a);\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");

	EYE_vec3 = glGetUniformLocation(program, "EYE");

	LIGHT_TYPE_int = glGetUniformLocation(program, "LIGHT_TYPE");
	LIGHT_LOCATION_vec3 = glGetUniformLocation(program, "LIGHT_LOCATION");
	LIGHT_DIRECTION_vec3 = glGetUniformLocation(program, "LIGHT_DIRECTION");
	LIGHT_ENERGY_vec3 = glGetUniformLocation(program, "LIGHT_ENERGY");
	LIGHT_CUTOFF_float = glGetUniformLocation(program, "LIGHT_CUTOFF");

	GLuint POSITION_TEX_sampler2D = glGetUniformLocation(program, "POSITION_TEX");
	GLuint NORMAL_ROUGHNESS_TEX_sampler2D = glGetUniformLocation(program, "NORMAL_ROUGHNESS_TEX");
	GLuint ALBEDO_TEX_sampler2D = glGetUniformLocation(program, "ALBEDO_TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(POSITION_TEX_sampler2D, 0); //set POSITION_TEX to sample from GL_TEXTURE0
	glUniform1i(NORMAL_ROUGHNESS_TEX_sampler2D, 1); //set NORMAL_ROUGHNESS_TEX to sample from GL_TEXTURE1
	glUniform1i(ALBEDO_TEX_sampler2D, 2); //set ALBEDO_TEX to sample from GL_TEXTURE2

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

BasicMaterialDeferredLightProgram::~BasicMaterialDeferredLightProgram() {
	glDeleteProgram(program);
	program = 0;
}

