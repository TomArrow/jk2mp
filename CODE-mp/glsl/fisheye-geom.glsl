#version 400 compatibility

// Geometry Shader
#extension GL_ARB_geometry_shader4 : enable
//in float realDepth[3];
float realDepth[3]; in vec4 color[3];
out vec4 vertColor;

in vec4 geomTexCoord[3];

in vec4 gl_TexCoordIn[3][1];

uniform vec3 dofJitterUniform;
uniform float dofFocusUniform;
uniform float dofRadiusUniform;
uniform int fishEyeModeUniform; //1= fisheye, 2=equirectangular
uniform float fovXUniform;
uniform float fovYUniform;
uniform int pixelWidthUniform;
uniform int pixelHeightUniform;

//
//
// General stuff
//
//
float angleOnPlane(vec3 point, vec3 axis1, vec3 axis2)
{

	vec2 planePosition = vec2(dot(point, axis1), dot(point, axis2));
	return acos(dot(vec2(1, 0), normalize(planePosition)));
}

vec3 getPerpendicularAxis(vec3 point, vec3 mainAxis)
{
	return normalize(point - dot(point, mainAxis) *mainAxis);
}


float getNormalSign(vec3 observer,vec3 triangle[3]){
    vec3 pseudoAxis  = normalize(triangle[1]-triangle[0]);
	vec3 pseudoPoint = normalize(triangle[2]-triangle[0]);
	vec3 relativeObserver = observer - triangle[0];
	vec3 rightAngleAxisToThirdPoint= getPerpendicularAxis(pseudoPoint,pseudoAxis);
	vec3 normal = cross(rightAngleAxisToThirdPoint,pseudoAxis);
	return sign(dot(normal,relativeObserver));
}


void setDebugColor(float x, float y, float z)
{
	//debugColor = vec3(max(0.0,min(1.0,x)),max(0.0,min(1.0,y)),max(0.0,min(1.0,z)));
}

float calcDofFactor(float pointDistance, float dofFocus)
{
	return abs(pointDistance - dofFocus);	// /pointDistance;
}

//
//
// Equirectangular stuff.
//
//
vec4 equirect_getPos(vec3 pointVec, int iter)
{

	float pi = radians(180);

	vec3 axis[3];
	axis[0] = vec3(0.0, 0.0, -1.0);
	axis[1] = vec3(-1.0, 0.0, 0.0);
	axis[2] = vec3(0.0, 1.0, 0.0);

	float distance = length(pointVec);

	vec3 perpendicularZAxisToPoint = getPerpendicularAxis(normalize(pointVec), axis[2].xyz);

	// Apply DOF jitter
	float dofFactor = calcDofFactor(distance, dofFocusUniform) *dofRadiusUniform *0.001;	// 0.001 to get it roughly in line with normal dof in mme. 
	vec3 perpendicularXAxisToPoint = cross(axis[2].xyz, perpendicularZAxisToPoint);
	pointVec.x += dofFactor* dot(dofJitterUniform, perpendicularXAxisToPoint);
	pointVec.y += dofFactor* dot(dofJitterUniform, axis[2].xyz);
	pointVec.z += dofFactor* dot(dofJitterUniform, perpendicularZAxisToPoint);

	pointVec = normalize(pointVec);

	vec4 outputPos;

	float depth = dot(axis[0].xyz, pointVec);
	realDepth[iter] = depth;

	float xAngle = angleOnPlane(pointVec, -axis[1].xyz, axis[0].xyz) / pi;

	float yAngle = angleOnPlane(pointVec, axis[2].xyz, perpendicularZAxisToPoint) / pi;

	xAngle -= 0.5;
	xAngle *= 2;
	float widthSign = sign(xAngle);
	xAngle = depth <= 0 ? xAngle : widthSign *(1.0 + (1.0 - abs(xAngle)));
	xAngle *= 0.5;

	yAngle -= 0.5;
	yAngle *= 2;
	float heightSign = sign(yAngle);

	outputPos.x = xAngle;
	outputPos.y = yAngle;
	outputPos.z = 1.0 - 1.0 / distance;
	outputPos.w = 1.0;

	return outputPos;
}



void equirect(){
	bool someInBack = false;
	bool someLeft = false;
	bool someRight = false;

	vec4 positions[3];
	for (int i = 0; i < 3; i++)
	{
		positions[i] = equirect_getPos(gl_PositionIn[i].xyz, i);
	}

	for (int i = 0; i < 3; i++)
	{
		if (realDepth[i] > 0) someInBack = true;
		if (positions[i].x <= 0) someLeft = true;
		if (positions[i].x > 0) someRight = true;
	}

	bool wrappedAround = false;
	if (someInBack && someLeft && someRight)
	{
		wrappedAround = true;
	}

	if (!wrappedAround)
	{
		for (int i = 0; i < 3; i++)
		{
			gl_Position = positions[i];
			gl_TexCoord[0] = gl_TexCoordIn[i][0];
			gl_TexCoord[0] = geomTexCoord[i];
			vertColor = color[i];
			EmitVertex();
		}
		EndPrimitive();
	}
	else
	{
		// Emit 2 separate vertices.
		for (int i = 0; i < 3; i++)
		{
			vec4 thisPosition = positions[i];
			if (realDepth[i] > 0)
			{
				if (thisPosition.x <= 0) thisPosition.x += 2.0;
			}
			gl_Position = thisPosition;
			gl_TexCoord[0] = gl_TexCoordIn[i][0];
			gl_TexCoord[0] = geomTexCoord[i];
			vertColor = color[i];
			EmitVertex();
		}
		EndPrimitive();
		for (int i = 0; i < 3; i++)
		{
			vec4 thisPosition = positions[i];
			if (realDepth[i] > 0)
			{
				if (thisPosition.x > 0) thisPosition.x -= 2.0;
			}
			gl_Position = thisPosition;
			gl_TexCoord[0] = gl_TexCoordIn[i][0];
			gl_TexCoord[0] = geomTexCoord[i];
			vertColor = color[i];
			EmitVertex();
		}
		EndPrimitive();
	}
}

//
//
// Real fisheye stuff
//
//
vec4 fisheye_getPos(vec3 pointVec, int iter)
{

	float pi = radians(180);

	vec3 axis[3];
	axis[0] = vec3(0.0, 0.0, -1.0);
	axis[1] = vec3(-1.0, 0.0, 0.0);
	axis[2] = vec3(0.0, 1.0, 0.0);

	float distance = length(pointVec);

	vec3 perpendicularZAxisToPoint = getPerpendicularAxis(normalize(pointVec), axis[2].xyz); // TODO add HQ mode maybe where we calculate this again since after jitter it's technically no longer correct?

	// Apply DOF jitter
	float dofFactor = calcDofFactor(distance, dofFocusUniform) *dofRadiusUniform *0.001;	// 0.001 to get it roughly in line with normal dof in mme. 
	vec3 perpendicularXAxisToPoint = cross(axis[2].xyz, perpendicularZAxisToPoint);
	pointVec.x += dofFactor* dot(dofJitterUniform, perpendicularXAxisToPoint);
	pointVec.y += dofFactor* dot(dofJitterUniform, axis[2].xyz);
	pointVec.z += dofFactor* dot(dofJitterUniform, perpendicularZAxisToPoint);

	pointVec = normalize(pointVec);

	vec4 outputPos;

	float depth = dot(axis[0].xyz, pointVec);
	realDepth[iter] = depth;

	vec3 perpendicularRotatedAxisToPoint = getPerpendicularAxis(pointVec, axis[0].xyz);
	float angleFromCenter = angleOnPlane(pointVec, -axis[0].xyz, perpendicularRotatedAxisToPoint)/pi; // Distance from center, basically.
	vec2 pseudoAngleOnXYplane = normalize(vec2(dot(pointVec, -axis[1].xyz),dot(pointVec, axis[2].xyz)));

	//float widthSign = sign(dot(pointVec,-axis[1].xyz));
	//angleFromCenter = depth <= 0 ? angleFromCenter : widthSign *(1.0 + (1.0 - abs(xAngle)));

	vec2 position = pseudoAngleOnXYplane*angleFromCenter;
	float positionX = -position.x*360.0/fovXUniform/2.0;
	float positionY = -position.y*360.0/fovYUniform/2.0;

	/*

	float xAngle = angleOnPlane(pointVec, -axis[1].xyz, axis[0].xyz) / pi;

	float yAngle = angleOnPlane(pointVec, axis[2].xyz, perpendicularZAxisToPoint) / pi;

	xAngle -= 0.5;
	xAngle *= 2;
	float widthSign = sign(xAngle);
	xAngle = depth <= 0 ? xAngle : widthSign *(1.0 + (1.0 - abs(xAngle)));
	xAngle *= 0.5;

	yAngle -= 0.5;
	yAngle *= 2;
	float heightSign = sign(yAngle);
	*/
	outputPos.x = positionX;
	outputPos.y = positionY;
	outputPos.z = 1.0 - 1.0 / distance;
	outputPos.w = 1.0;

	return outputPos;
}

void fisheye(){
	bool someInBack = false;
	bool someLeft = false;
	bool someRight = false;

	vec4 positions[3];
	vec3 originalPositions3[3];
	vec3 positions3[3];
	for (int i = 0; i < 3; i++)
	{
		originalPositions3[i] = gl_PositionIn[i].xyz;
		positions[i] = fisheye_getPos(gl_PositionIn[i].xyz, i);
		positions3[i] = positions[i].xyz;
	}

	float originalSign = getNormalSign(vec3(0,0,0),originalPositions3);
	float newSign = getNormalSign(vec3(0,0,0),positions3);

	bool wrappedAround = originalSign != newSign;

	if (!wrappedAround)
	{
		for (int i = 0; i < 3; i++)
		{
			gl_Position = positions[i];
			gl_TexCoord[0] = gl_TexCoordIn[i][0];
			gl_TexCoord[0] = geomTexCoord[i];
			vertColor = color[i];
			EmitVertex();
		}
		EndPrimitive();
	}
}


void main()
{
	if(fishEyeModeUniform == 2){
		equirect();
	} else {
		fisheye();
	}
}