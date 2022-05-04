// Geometry Shader
#extension GL_ARB_geometry_shader4 : enable
//in float realDepth[3];
float realDepth[3]; in vec4 color[3];
out vec4 vertColor;

in vec4 geomTexCoord[3];

uniform vec3 dofJitterUniform;
uniform float dofFocusUniform;
uniform float dofRadiusUniform;

float angleOnPlane(vec3 point, vec3 axis1, vec3 axis2)
{

	vec2 planePosition = vec2(dot(point, axis1), dot(point, axis2));
	return acos(dot(vec2(1, 0), normalize(planePosition)));
}

vec3 getPerpendicularAxis(vec3 point, vec3 mainAxis)
{
	return normalize(point - dot(point, mainAxis) *mainAxis);
}

void setDebugColor(float x, float y, float z)
{
	//debugColor = vec3(max(0.0,min(1.0,x)),max(0.0,min(1.0,y)),max(0.0,min(1.0,z)));
}

float calcDofFactor(float pointDistance, float dofFocus)
{
	return abs(pointDistance - dofFocus);	// /pointDistance;
}

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

void main()
{
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