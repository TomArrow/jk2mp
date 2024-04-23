#include "tr_glsl.h"

#include <string>
#include <fstream>

R_GLSL::R_GLSL(char* filenameVertexShader, char* filenameTessellationControlShader, char* filenameTessellationEvaluationShader, char* filenameGeometryShader, char* filenameFragmentShader, qboolean noFragment) {

	bool doGeometryShader = strlen(filenameGeometryShader) && glConfig.geometryShaderARBAvailable;

	// No point doing the tessellation if we don't have a geometry shader, since there's no way to take advantage of the improved subdivision then
	bool doTessellationShader = doGeometryShader && glConfig.tesselationShaderAvailable && strlen(filenameTessellationControlShader) && strlen(filenameTessellationEvaluationShader);

	//shaderId =
	const char* vertexText;
	const char* geometryText;
	const char* tessellationControlText;
	const char* tessellationEvaluationText;
	const char* fragmentText;
	char* fragmentTextPerlinFuckery;
	bool success = true;
	try {
		vertexText = (new std::string(std::istreambuf_iterator<char>(std::ifstream(filenameVertexShader).rdbuf()), std::istreambuf_iterator<char>()))->c_str();
		fragmentText = (new std::string(std::istreambuf_iterator<char>(std::ifstream(filenameFragmentShader).rdbuf()), std::istreambuf_iterator<char>()))->c_str();
		int fragmentLength = strlen(fragmentText)+1;
		fragmentTextPerlinFuckery = new char[fragmentLength + 1];
		for (int i = fragmentLength-1; i >= 0; i--) {
			fragmentTextPerlinFuckery[i] = fragmentText[i];
			if (fragmentTextPerlinFuckery[i] == '#' && !_strnicmp(fragmentTextPerlinFuckery+i,"#define PERLINFVCKERY",sizeof("#define PERLINFVCKERY")-1)) {
				fragmentTextPerlinFuckery[i + strlen("#define PERLINF")] = 'U';
			}
		}
		if (doGeometryShader) {
			geometryText = (new std::string(std::istreambuf_iterator<char>(std::ifstream(filenameGeometryShader).rdbuf()), std::istreambuf_iterator<char>()))->c_str();
		}
		if (doTessellationShader) {
			tessellationControlText = (new std::string(std::istreambuf_iterator<char>(std::ifstream(filenameTessellationControlShader).rdbuf()), std::istreambuf_iterator<char>()))->c_str();
			tessellationEvaluationText = (new std::string(std::istreambuf_iterator<char>(std::ifstream(filenameTessellationEvaluationShader).rdbuf()), std::istreambuf_iterator<char>()))->c_str();
		}
	}
	catch (...) {
		success = false;
		if (doGeometryShader) {
			ri.Printf(PRINT_WARNING, "WARNING: Reading glsl shader files %s, %s and %s failed.\n", filenameVertexShader, filenameGeometryShader, filenameFragmentShader);
		}
		else {

			ri.Printf(PRINT_WARNING, "WARNING: Reading glsl shader files %s and %s failed.\n", filenameVertexShader, filenameFragmentShader);
		}
	}

	if (success == false) {
		delete fragmentTextPerlinFuckery;
		return;
	}




	GLuint vertexShaderId = qglCreateShader(GL_VERTEX_SHADER);
	ri.Printf(PRINT_WARNING, "DEBUG: Vertex shader id is %d.\n", (int)vertexShaderId);
	qglShaderSource(vertexShaderId, 1, &vertexText, NULL);
	qglCompileShader(vertexShaderId);
	if (hasErrored(vertexShaderId, filenameVertexShader, false)) {
		success = false;
	}

	GLuint geometryShaderId;
	if (doGeometryShader) {
		geometryShaderId = qglCreateShader(GL_GEOMETRY_SHADER_ARB);
		ri.Printf(PRINT_WARNING, "DEBUG: Geometry shader id is %d.\n", (int)geometryShaderId);
		qglShaderSource(geometryShaderId, 1, &geometryText, NULL);
		qglCompileShader(geometryShaderId);
		if (hasErrored(geometryShaderId, filenameGeometryShader, false)) {
			success = false;
		}
	}

	GLuint tessellationControlShaderId;
	GLuint tessellationEvaluationShaderId;
	if (doTessellationShader) {
		tessellationControlShaderId = qglCreateShader(GL_TESS_CONTROL_SHADER);
		ri.Printf(PRINT_WARNING, "DEBUG: Tessellation control shader id is %d.\n", (int)tessellationControlShaderId);
		qglShaderSource(tessellationControlShaderId, 1, &tessellationControlText, NULL);
		qglCompileShader(tessellationControlShaderId);
		if (hasErrored(tessellationControlShaderId, filenameTessellationControlShader, false)) {
			success = false;
		}

		tessellationEvaluationShaderId = qglCreateShader(GL_TESS_EVALUATION_SHADER);
		ri.Printf(PRINT_WARNING, "DEBUG: Tessellation evaluation shader id is %d.\n", (int)tessellationEvaluationShaderId);
		qglShaderSource(tessellationEvaluationShaderId, 1, &tessellationEvaluationText, NULL);
		qglCompileShader(tessellationEvaluationShaderId);
		if (hasErrored(tessellationEvaluationShaderId, filenameTessellationEvaluationShader, false)) {
			success = false;
		}
	}

	GLuint fragmentShaderId = qglCreateShader(GL_FRAGMENT_SHADER);
	ri.Printf(PRINT_WARNING, "DEBUG: Fragment shader id is %d.\n", (int)fragmentShaderId);
	qglShaderSource(fragmentShaderId, 1, &fragmentText, NULL);
	qglCompileShader(fragmentShaderId);
	if (hasErrored(fragmentShaderId, filenameFragmentShader, false)) {
		success = false;
	}

	GLuint fragmentShaderPerlinId = qglCreateShader(GL_FRAGMENT_SHADER);
	ri.Printf(PRINT_WARNING, "DEBUG: Fragment shader (perlin fuckery) id is %d.\n", (int)fragmentShaderPerlinId);
	//const char* withPerlin[2] = {"#define PERLINFUCKERY\n",fragmentText };
	//std::string perlinFragment = fragmentText.rep
	const char* tmp = fragmentTextPerlinFuckery;
	qglShaderSource(fragmentShaderPerlinId, 1, &tmp, NULL);
	qglCompileShader(fragmentShaderPerlinId);
	if (hasErrored(fragmentShaderPerlinId, filenameFragmentShader, false)) {
		success = false;
	}

	// Normal shader
	shaderId = qglCreateProgram();
	ri.Printf(PRINT_WARNING, "DEBUG: Program shader id is %d.\n", (int)shaderId);
	qglAttachShader(shaderId, vertexShaderId);
	if (doTessellationShader) {
		qglAttachShader(shaderId, tessellationControlShaderId);
		qglAttachShader(shaderId, tessellationEvaluationShaderId);
	}
	if (doGeometryShader) {
		qglAttachShader(shaderId, geometryShaderId);
		qglProgramParameteri(shaderId, GL_GEOMETRY_VERTICES_OUT_ARB, 6);
		qglProgramParameteri(shaderId, GL_GEOMETRY_INPUT_TYPE_ARB, GL_TRIANGLES);
		qglProgramParameteri(shaderId, GL_GEOMETRY_OUTPUT_TYPE_ARB, GL_TRIANGLE_STRIP);
	}
	if (!noFragment) {
		qglAttachShader(shaderId, fragmentShaderId);
	}
	qglLinkProgram(shaderId);
	if (hasErrored(shaderId, "[shader program]", true)) {
		qglDeleteProgram(shaderId);
		shaderId = 0;
		success = false;
	}

	// Shader with perlin fuckery (thanks AMD for not letting me include it in the normal shader program because you like to create graphical artifacts from code that is literally never executed)
	shaderIdPerlinFuckery = qglCreateProgram();
	ri.Printf(PRINT_WARNING, "DEBUG: Program shader (perlin fuckery) id is %d.\n", (int)shaderId);
	qglAttachShader(shaderIdPerlinFuckery, vertexShaderId);
	if (doTessellationShader) {
		qglAttachShader(shaderIdPerlinFuckery, tessellationControlShaderId);
		qglAttachShader(shaderIdPerlinFuckery, tessellationEvaluationShaderId);
	}
	if (doGeometryShader) {
		qglAttachShader(shaderIdPerlinFuckery, geometryShaderId);
		qglProgramParameteri(shaderIdPerlinFuckery, GL_GEOMETRY_VERTICES_OUT_ARB, 6);
		qglProgramParameteri(shaderIdPerlinFuckery, GL_GEOMETRY_INPUT_TYPE_ARB, GL_TRIANGLES);
		qglProgramParameteri(shaderIdPerlinFuckery, GL_GEOMETRY_OUTPUT_TYPE_ARB, GL_TRIANGLE_STRIP);
	}
	if (!noFragment) {
		qglAttachShader(shaderIdPerlinFuckery, fragmentShaderPerlinId);
	}
	qglLinkProgram(shaderIdPerlinFuckery);
	if (hasErrored(shaderId, "[shader program perlin]", true)) {
		qglDeleteProgram(shaderIdPerlinFuckery);
		shaderIdPerlinFuckery = 0;
		success = false;
	}


	if (doGeometryShader) {
		qglDeleteShader(geometryShaderId);
	}
	if (doTessellationShader) {
		qglDeleteShader(tessellationControlShaderId);
		qglDeleteShader(tessellationEvaluationShaderId);
	}
	qglDeleteShader(vertexShaderId);
	qglDeleteShader(fragmentShaderId);

	//if (!success && shaderId) {
		//qglDeleteProgram(shaderId);
	//}

	isWorking = success;
	delete fragmentTextPerlinFuckery;
}



bool R_GLSL::hasErrored(GLuint glId, char* filename, bool isProgram) {

	GLint tmp;
	char errorMessage[MAX_SHADER_ERROR_LOG_LENGTH];
	if (!isProgram) {
		qglGetShaderiv(glId, GL_COMPILE_STATUS,&tmp);
		if (!tmp) {
			qglGetShaderInfoLog(glId, MAX_SHADER_ERROR_LOG_LENGTH, NULL, errorMessage);
			ri.Printf(PRINT_WARNING, "WARNING: Shader compilation of %d (%s) failed: %s.\n", (int)glId, filename, errorMessage);
		}
	}
	else {
		qglGetProgramiv(glId, GL_LINK_STATUS, &tmp);
		if (!tmp) {
			qglGetProgramInfoLog(glId, MAX_SHADER_ERROR_LOG_LENGTH, NULL, errorMessage);
			ri.Printf(PRINT_WARNING, "WARNING: Shader compilation of %d (%s) failed: %s.\n", (int)glId, filename, errorMessage);
		}
	}
	return !tmp;
}