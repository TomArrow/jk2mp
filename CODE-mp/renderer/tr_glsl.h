#pragma once
#include "tr_local.h"

#define MAX_SHADER_ERROR_LOG_LENGTH 2048

class R_GLSL
{
public:
	GLuint ShaderId(bool perlinFuckery) {
		return perlinFuckery ? shaderIdPerlinFuckery: shaderId;
	};
	bool IsWorking() {
		return isWorking;
	};
	R_GLSL(char* filenameVertexShader, char* filenameTessellationControlShader, char* filenameTessellationEvaluationShader, char* filenameGeometryShader, char* filenameFragmentShader, qboolean noFragment);
	~R_GLSL(){
		if (shaderId) {
			qglDeleteProgram(shaderId);
		}
		if (shaderIdPerlinFuckery) {
			qglDeleteProgram(shaderIdPerlinFuckery);
		}
	}
private:
	GLuint shaderId;
	GLuint shaderIdPerlinFuckery;
	bool isWorking = false;
	bool hasErrored(GLuint glId,char* filename,bool isProgram); //true if has errored
};

