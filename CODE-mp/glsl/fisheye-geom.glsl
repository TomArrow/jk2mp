#version 400 compatibility

// Geometry Shader
#extension GL_ARB_geometry_shader4 : enable
//in float realDepth[3];
float realDepth[3]; in vec4 color[3];
varying out vec4 vertColor;
out vec3 debugColor;
out vec3 texUVTransform[2];

in vec4 geomTexCoord[3];

in vec4 gl_TexCoordIn[3][1];

in mat4x4 projectionMatrix[3];


in mat4x4 worldModelViewMatrixReverse[3];
out mat4x4 worldModelViewMatrixReverseGeom;

out vec3 normal;

varying in vec4 eyeSpaceCoords[3];
varying out vec4 eyeSpaceCoordsGeom;

varying in vec4 pureVertexCoords[3];
varying out vec4 pureVertexCoordsGeom;

uniform vec3 pixelJitterUniform;
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
	debugColor = vec3(max(0.0,min(1.0,x)),max(0.0,min(1.0,y)),max(0.0,min(1.0,z)));
}

float calcDofFactor(float pointDistance, float dofFocus)
{
	return abs(pointDistance - dofFocus);	// /pointDistance;
}

vec3 generateTransformationMatrixRow(in vec3 vec1i, in vec3 vec2i, in vec3 vec3i, float resultValue1, float resultValue2, float resultValue3){
	vec3 transformVec;
	
	transformVec.x = (vec2i.z * vec3i.y * resultValue1 - vec2i.y * vec3i.z * resultValue1 - vec1i.z * vec3i.y * resultValue2 + vec1i.y * vec3i.z * resultValue2 + vec1i.z * vec2i.y * resultValue3 - vec1i.y * vec2i.z * resultValue3) / (vec1i.z * vec2i.y * vec3i.x - vec1i.y * vec2i.z * vec3i.x - vec1i.z * vec2i.x * vec3i.y + vec1i.x * vec2i.z * vec3i.y + vec1i.y * vec2i.x * vec3i.z - vec1i.x * vec2i.y * vec3i.z);
	transformVec.y = (-vec2i.z * vec3i.x * resultValue1 + vec2i.x * vec3i.z * resultValue1 + vec1i.z * vec3i.x * resultValue2 - vec1i.x * vec3i.z * resultValue2 - vec1i.z * vec2i.x * resultValue3 + vec1i.x * vec2i.z * resultValue3) / (vec1i.z * vec2i.y * vec3i.x - vec1i.y * vec2i.z * vec3i.x - vec1i.z * vec2i.x * vec3i.y + vec1i.x * vec2i.z * vec3i.y + vec1i.y * vec2i.x * vec3i.z - vec1i.x * vec2i.y * vec3i.z);
	transformVec.z = (-vec2i.y * vec3i.x * resultValue1 + vec2i.x * vec3i.y * resultValue1 + vec1i.y * vec3i.x * resultValue2 - vec1i.x * vec3i.y * resultValue2 - vec1i.y * vec2i.x * resultValue3 + vec1i.x * vec2i.y * resultValue3) / (-vec1i.z * vec2i.y * vec3i.x + vec1i.y * vec2i.z * vec3i.x + vec1i.z * vec2i.x * vec3i.y - vec1i.x * vec2i.z * vec3i.y - vec1i.y * vec2i.x * vec3i.z + vec1i.x * vec2i.y * vec3i.z);
	
	return transformVec;
}

// Calculates matrix from 2 pairs of vectors that transforms the i vec3 ones to the o vec2 ones.
void makeUVTransformationMatrix(in vec3 vec1i, in vec2 vec1o, in vec3 vec2i, in vec2 vec2o, in vec3 vec3i, in vec2 vec3o, inout vec3 matrix[2]){
	
	matrix[0] = generateTransformationMatrixRow(vec1i,vec2i,vec3i,vec1o.s,vec2o.s,vec3o.s);
	matrix[1] = generateTransformationMatrixRow(vec1i,vec2i,vec3i,vec1o.t,vec2o.t,vec3o.t);

}

//
//
// Standard stuff 
//
//
void standard(){

	normal = normalize(cross(normalize(gl_PositionIn[2].xyz-gl_PositionIn[0].xyz),normalize(gl_PositionIn[1].xyz-gl_PositionIn[0].xyz)));

	// Calculate UV vectors (dunno what im doing, i want parallax mapping lol)
	vec3 uvtransform[2];
	makeUVTransformationMatrix(gl_PositionIn[0].xyz,geomTexCoord[0].st,gl_PositionIn[1].xyz,geomTexCoord[1].st,gl_PositionIn[2].xyz,geomTexCoord[2].st,uvtransform);
	texUVTransform= uvtransform;

	//setDebugColor(1,0,0);
	for (int i = 0; i < 3; i++)
	{
		gl_Position = projectionMatrix[0]* gl_PositionIn[i];
		//gl_TexCoord[0] = gl_TexCoordIn[i][0];
		gl_TexCoord[0] = geomTexCoord[i];
		//gl_TexCoord[0].s = dot(gl_PositionIn[i].xyz,uvtransform[0]);
		//gl_TexCoord[0].t = dot(gl_PositionIn[i].xyz,uvtransform[1]);
		eyeSpaceCoordsGeom = eyeSpaceCoords[i];
		pureVertexCoordsGeom = pureVertexCoords[i];

		vertColor = color[i];
		EmitVertex();
	}
	EndPrimitive();

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
	if(dofFactor != 0.0){
		vec3 perpendicularXAxisToPoint = cross(axis[2].xyz, perpendicularZAxisToPoint);
		pointVec.x += dofFactor* dot(dofJitterUniform, perpendicularXAxisToPoint);
		pointVec.y += dofFactor* dot(dofJitterUniform, axis[2].xyz);
		pointVec.z += dofFactor* dot(dofJitterUniform, perpendicularZAxisToPoint);
		pointVec = normalize(pointVec);
		perpendicularZAxisToPoint = getPerpendicularAxis(pointVec, axis[2].xyz); // Calculate it again because position has changed through jitter.
	} else {
		pointVec = normalize(pointVec);
	}


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
	
	// This is pixel jitter. It's applied for atni-aliasing, hence it's the last step and applied onto actual screen coordinates
	outputPos.x += pixelJitterUniform.x;
	outputPos.y += pixelJitterUniform.y;
	outputPos.z += pixelJitterUniform.z;

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
vec4 fisheye_getPos(vec3 pointVec, int iter, inout vec2 fovless2DPos)
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
	// old faulty way:
	//pointVec.x += dofFactor* dot(dofJitterUniform, perpendicularXAxisToPoint);
	//pointVec.y += dofFactor* dot(dofJitterUniform, axis[2].xyz);
	//pointVec.z += dofFactor* dot(dofJitterUniform, perpendicularZAxisToPoint);
	// This might still be wrong, not sure.  (yeah it doesnt angle vertically i think)
	//pointVec += dofFactor * dofJitterUniform.x * perpendicularXAxisToPoint;
	//pointVec += dofFactor * dofJitterUniform.y * axis[2].xyz;
	//pointVec += dofFactor * dofJitterUniform.z * perpendicularZAxisToPoint;
	// Hmm the getperpendicularaxis might actually not work if the two axes supplied are identical. Might result in Null vector. Do we just ignore that for now? Hmm
	vec3 pointPseudoZAxis = normalize(pointVec);
	vec3 pseudoYAxisToPoint = getPerpendicularAxis(axis[2].xyz, pointPseudoZAxis); 
	vec3 pseudoXAxisToPoint = cross(pointPseudoZAxis,pseudoYAxisToPoint);
	pointVec += dofFactor * dofJitterUniform.x * pseudoXAxisToPoint;
	pointVec += dofFactor * dofJitterUniform.y * pseudoYAxisToPoint;
	pointVec += dofFactor * dofJitterUniform.z * pointPseudoZAxis;

	pointVec = normalize(pointVec);

	vec4 outputPos;

	float depth = dot(axis[0].xyz, pointVec);
	realDepth[iter] = depth;

	vec3 perpendicularRotatedAxisToPoint = getPerpendicularAxis(pointVec, axis[0].xyz);
	float angleFromCenter = angleOnPlane(pointVec, -axis[0].xyz, perpendicularRotatedAxisToPoint)/pi; // Distance from center, basically.
	vec2 pseudoAngleOnXYplane = vec2(dot(pointVec, -axis[1].xyz),dot(pointVec, axis[2].xyz));


	pseudoAngleOnXYplane = normalize(pseudoAngleOnXYplane);


	vec2 position = pseudoAngleOnXYplane*angleFromCenter;
	fovless2DPos = position;
	float positionX = -position.x*360.0/fovXUniform/2.0;
	//float positionY = -position.y*360.0/fovYUniform/2.0; // TODO do a proper fix to make it use the fovY so it will be correct underwater etc. It's calculated in a weird way tho.
	float positionY = -position.y*360.0/fovXUniform/pixelHeightUniform*pixelWidthUniform/2.0;

	outputPos.x = positionX;
	outputPos.y = positionY;
	outputPos.z = 1.0 - 1.0 / distance;
	outputPos.w = 1.0;

	// This is pixel jitter. It's applied for atni-aliasing, hence it's the last step and applied onto actual screen coordinates
	outputPos.x += pixelJitterUniform.x;
	outputPos.y += pixelJitterUniform.y;
	outputPos.z += pixelJitterUniform.z;

	return outputPos;
}

void fisheye(){
	int inBack = 0;
	//bool someLeft = false;
	//bool someRight = false;

	vec4 positions[3];
	vec3 originalPositions3[3];
	vec3 positions3[3];
	//vec3 preFisheye2DPos[3];
	vec3 postFisheye2DPos[3];
	for (int i = 0; i < 3; i++)
	{
		originalPositions3[i] = gl_PositionIn[i].xyz;
		positions[i] = fisheye_getPos(gl_PositionIn[i].xyz, i,postFisheye2DPos[i].xy);
		positions3[i] = positions[i].xyz;
		//postFisheye2DPos[i] = vec3( positions[i].xy,-1);
		postFisheye2DPos[i].z = -1;
	}

	// Some attempt to get rid of artifacts...
	float longestSidePost = max(max(length(postFisheye2DPos[0].xyz-postFisheye2DPos[1].xyz),length(postFisheye2DPos[1].xyz-postFisheye2DPos[2].xyz)),length(postFisheye2DPos[2].xyz-postFisheye2DPos[0].xyz));


	for (int i = 0; i < 3; i++)
	{
		if (realDepth[i] > 0) inBack++;
	}

	
	//float originalSign = getNormalSign(vec3(0,0,0),originalPositions3);
	//float newSign = getNormalSign(vec3(0,0,0),positions3);

	//setDebugColor(longestSidePost/10,sizeRatio,relativeLongestSide/100);
	bool wrappedAround = false;// originalSign != newSign;// || original2DSign != new2DSign;

	if (!(wrappedAround || (longestSidePost > 1.0f))) // The longest side thing feels like a dirty hack. But it kidna works. And can be further improved with better TCS I think.
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
	worldModelViewMatrixReverseGeom = worldModelViewMatrixReverse[0];
// TODO Apply distortion in TES instead? To have more multithreading? And then send it over as in/out variable?
	if(fishEyeModeUniform == 2){
		equirect();
	} else if(fishEyeModeUniform == 1){
		fisheye();
	} else {
		standard();
	}
}