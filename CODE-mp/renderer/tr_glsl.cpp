#include "tr_glsl.h"

#include <string>
#include <fstream>

R_GLSL::R_GLSL(char* filenameVertexShader, char* filenameFragmentShader) {

	//shaderId =
	const char* vertexText;
	const char* fragmentText;
	bool success = true;
	try {
		vertexText = (new std::string(std::istreambuf_iterator<char>(std::ifstream(filenameVertexShader).rdbuf()), std::istreambuf_iterator<char>()))->c_str();
		fragmentText = (new std::string(std::istreambuf_iterator<char>(std::ifstream(filenameFragmentShader).rdbuf()), std::istreambuf_iterator<char>()))->c_str();
	}
	catch (...) {
		success = false;
		ri.Printf(PRINT_WARNING, "WARNING: Reading glsl shader files %s and %s failed.\n",filenameVertexShader,filenameFragmentShader);
	}

	if (success == false) {
		return;
	}

	GLuint vertexShaderId = qglCreateShader(GL_VERTEX_SHADER);
	ri.Printf(PRINT_WARNING, "DEBUG: Vertex shader id is %d.\n", (int)vertexShaderId);
	qglShaderSource(vertexShaderId, 1, &vertexText, NULL);
	qglCompileShader(vertexShaderId);
	if (hasErrored(vertexShaderId, filenameVertexShader, false)) {
		success = false;
	}

	GLuint fragmentShaderId = qglCreateShader(GL_FRAGMENT_SHADER);
	ri.Printf(PRINT_WARNING, "DEBUG: Fragment shader id is %d.\n", (int)fragmentShaderId);
	qglShaderSource(fragmentShaderId, 1, &fragmentText, NULL);
	qglCompileShader(fragmentShaderId);
	if (hasErrored(fragmentShaderId, filenameFragmentShader, false)) {
		success = false;
	}

	shaderId = qglCreateProgram();
	ri.Printf(PRINT_WARNING, "DEBUG: Program shader id is %d.\n", (int)shaderId);
	qglAttachShader(shaderId, vertexShaderId);
	qglAttachShader(shaderId, fragmentShaderId);
	qglLinkProgram(shaderId);
	if (hasErrored(shaderId, "[shader program]", true)) {
		success = false;
	}

	qglDeleteShader(vertexShaderId);
	qglDeleteShader(fragmentShaderId);

	isWorking = success;
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