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
	bool success = true;
	try {
		vertexText = (new std::string(std::istreambuf_iterator<char>(std::ifstream(filenameVertexShader).rdbuf()), std::istreambuf_iterator<char>()))->c_str();
		fragmentText = (new std::string(std::istreambuf_iterator<char>(std::ifstream(filenameFragmentShader).rdbuf()), std::istreambuf_iterator<char>()))->c_str();
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