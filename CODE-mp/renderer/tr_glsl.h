#pragma once
#include "tr_local.h"

#define MAX_SHADER_ERROR_LOG_LENGTH 2048

class R_GLSL
{
public:
	GLuint ShaderId() {
		return shaderId;
	};
	bool IsWorking() {
		return isWorking;
	};
	R_GLSL(char* filenameVertexShader, char* filenameFragmentShader);
private:
	GLuint shaderId;
	bool isWorking;
	bool hasErrored(GLuint glId,char* filename,bool isProgram); //true if has errored
};

