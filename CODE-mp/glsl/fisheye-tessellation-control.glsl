#version 410 core
#extension GL_ARB_tessellation_shader : enable

layout(vertices = 3) out;

in vec4 texCoord[];
in vec4 colorVertex[];
out vec4 vertexTexCoord[];
out vec4 colorTCS[];

void main()
{


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

 
	if(gl_InvocationID==0){
		
		float maxSideLength = 50;

		float maxLevel = 1.0;
		// TODO make it find not only closest point distance, but also how close the plane that it creates comes to the camera.
		for(int i=0;i<3;i++){
			//float closestDistance = min(min(length(gl_in[i].gl_Position.xyz),length(gl_in[(i+1)%3].gl_Position.xyz)),min(length(gl_in[i].gl_Position.xz),length(gl_in[(i+1)%3].gl_Position.xz)));
			float closestDistance =min(length(gl_in[i].gl_Position.xyz),length(gl_in[(i+1)%3].gl_Position.xyz));
			float maxSideLengthHere = max(closestDistance/300.0*maxSideLength,5);
			float sideLength = length(gl_in[i].gl_Position-gl_in[(i+1)%3].gl_Position);
			float tessLevelHere = max(1,ceil(sideLength/maxSideLengthHere));
			gl_TessLevelOuter[(i+2)%3] = tessLevelHere;
			maxLevel = max(maxLevel,tessLevelHere);
		}

		//int vertCount = gl_in.length();
		gl_TessLevelInner[0] = maxLevel;

	}
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	vertexTexCoord[gl_InvocationID] = texCoord[gl_InvocationID];
	colorTCS[gl_InvocationID] = colorVertex[gl_InvocationID];
}