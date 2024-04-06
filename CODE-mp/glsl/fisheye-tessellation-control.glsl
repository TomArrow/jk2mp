#version 400 compatibility
#extension GL_ARB_tessellation_shader : enable

layout(vertices = 3) out;

in vec4 texCoord[];
in vec4 colorVertex[];
out vec4 vertexTexCoord[];
out vec4 colorTCS[];


in mat4x4 worldModelViewMatrixReverse[];
out mat4x4 worldModelViewMatrixReverseTCS[];

in vec3 normal[];
out vec3 normalTCS[];

in vec4 eyeSpaceCoords[];
out vec4 eyeSpaceCoordsTCS[];

in vec4 pureVertexCoords[];
out vec4 pureVertexCoordsTCS[];


vec3 getPerpendicularAxis(vec3 point, vec3 mainAxis)
{
	return normalize(point - dot(point, mainAxis) *mainAxis);
}

float distanceToSide(vec3 point,vec3 sidePointA, vec3 sidePointB){
	vec3 sideVector = sidePointB-sidePointA;
	float sideLength = length(sideVector);
	vec3 sideAxis = normalize(sideVector);
	vec3 relativePoint = point-sidePointA;
	vec3 normalizedPoint = normalize(relativePoint);
	float pointAlongSide = dot(sideAxis,relativePoint);
	if(pointAlongSide<sideLength && pointAlongSide > 0.0){
		vec3 perpAxis = getPerpendicularAxis(normalizedPoint,sideAxis);
		return dot(perpAxis,relativePoint);
	} else {
		// point is not along the line, so the closest distance is the distance to
		// the closer line point.
		return min(length(point-sidePointA),length(point-sidePointB));
	}

}

float distanceToTriangle(){
	return 100000; // Will do proper later.
}

void main()
{

// TODO Different/expanded algo for fisheye mode 1. Because needs potentially more subdiv in the back than in front etc.


 /*gl_TessLevelOuter[0] = 1.0;
 gl_TessLevelOuter[1] = 2.0;
 gl_TessLevelOuter[2] = 3.0;
 gl_TessLevelOuter[3] = 4.0;

 gl_TessLevelInner[0] = 1.0;
 gl_TessLevelInner[1] = 1.0;
 
 if(gl_in.length() == 3 && 3== gl_InvocationID){
	// I think we're getting triangles. But we wanna output quads.
	// Uhm. Let's just be crude and put the last point in between two other points.
	// TODO: Make the extra point go on the longest line.
	gl_out[gl_InvocationID].gl_Position = 0.5*gl_in[1].gl_Position + 0.5*gl_in[2].gl_Position;
	vertexTexCoord[gl_InvocationID] = texCoord[1]*0.5+texCoord[2]*0.5;
	colorTCS[gl_InvocationID] = colorVertex[1]*0.5+colorVertex[2]*0.5;
 } else {
	
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	vertexTexCoord[gl_InvocationID] = texCoord[gl_InvocationID];
	colorTCS[gl_InvocationID] = colorVertex[gl_InvocationID];
 }*/

 
	/*if(gl_InvocationID==0){
		
		float maxSideLength = 50;

		float maxLevel = 1.0;
		float longestSide = 0.0;
		bool anyDistanceShorterThanSideLength = false;
		// TODO make it find not only closest point distance, but also how close the plane that it creates comes to the camera.
		for(int i=0;i<3;i++){
			vec3 pointA = gl_in[i].gl_Position.xyz;
			vec3 pointB = gl_in[(i+1)%3].gl_Position.xyz;
			//float closestDistance = min(min(length(gl_in[i].gl_Position.xyz),length(gl_in[(i+1)%3].gl_Position.xyz)),min(length(gl_in[i].gl_Position.xz),length(gl_in[(i+1)%3].gl_Position.xz)));
			float closestDistance =min(length(pointA),length(pointB));
			float sideLength = length(pointA-pointB);
			if(closestDistance < sideLength){
				anyDistanceShorterThanSideLength = true;
				closestDistance = distanceToSide(vec3(0,0,0),pointA,pointB);
			}
			float maxSideLengthHere = max(closestDistance/300.0*maxSideLength,5);
			longestSide = max(longestSide,sideLength);
			float tessLevelHere = max(1,ceil(sideLength/maxSideLengthHere));
			gl_TessLevelOuter[(i+2)%3] = tessLevelHere;
			maxLevel = max(maxLevel,tessLevelHere);
		}

		*//*if(anyDistanceShorterThanSideLength){
			// This *should* apply mostly to very big close by triangles.
			// For those, make a more elaborate calculation from closest point to triangle.
			float trueDistance = distanceToTriangle();
			float maxSideLengthHere = max(trueDistance/300.0*maxSideLength,5);
			float tessLevel = max(1,ceil(longestSide/maxSideLengthHere));
			maxLevel = max(maxLevel,tessLevel);
			for(int i=0;i<3;i++){
				// TODO Problem: How does this guarantee consistent tessellation at edges? Hmm.
				gl_TessLevelOuter[i] = max(gl_TessLevelOuter[i],tessLevel);
			}
		}*//*

		//int vertCount = gl_in.length();
		gl_TessLevelInner[0] = maxLevel;

	}*/

	// Attempt to have more multithreading.
	float maxSideLength = 50;
	int i = gl_InvocationID;
	vec3 pointA = gl_in[i].gl_Position.xyz;
	vec3 pointB = gl_in[(i+1)%3].gl_Position.xyz;
	//float closestDistance = min(min(length(gl_in[i].gl_Position.xyz),length(gl_in[(i+1)%3].gl_Position.xyz)),min(length(gl_in[i].gl_Position.xz),length(gl_in[(i+1)%3].gl_Position.xz)));
	float closestDistance =min(length(pointA),length(pointB));
	float sideLength = length(pointA-pointB);
	if(closestDistance < sideLength){
		//anyDistanceShorterThanSideLength = true;
		closestDistance = distanceToSide(vec3(0,0,0),pointA,pointB);
	}
	float maxSideLengthHere = max(closestDistance/300.0*maxSideLength,5);
	//longestSide = max(longestSide,sideLength);
	float tessLevelHere = max(1,ceil(sideLength/maxSideLengthHere));
	gl_TessLevelOuter[(i+2)%3] = tessLevelHere;
	//maxLevel = max(maxLevel,tessLevelHere);



	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	vertexTexCoord[gl_InvocationID] = texCoord[gl_InvocationID];
	colorTCS[gl_InvocationID] = colorVertex[gl_InvocationID];


	worldModelViewMatrixReverseTCS[gl_InvocationID] = worldModelViewMatrixReverse[gl_InvocationID];
	normalTCS[gl_InvocationID] = normal[gl_InvocationID];
	eyeSpaceCoordsTCS[gl_InvocationID] = eyeSpaceCoords[gl_InvocationID];
	pureVertexCoordsTCS[gl_InvocationID] = pureVertexCoords[gl_InvocationID];


	
	barrier();


	if(gl_InvocationID == 0){

		gl_TessLevelInner[0] = max(max(gl_TessLevelOuter[0],gl_TessLevelOuter[1]),gl_TessLevelOuter[2]);
	}

	}