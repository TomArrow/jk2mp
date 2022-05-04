#version 410 core
#extension GL_ARB_tessellation_shader : enable

layout(vertices = 4) out;

void main()
{
 gl_TessLevelOuter[0] = 2.0;
 gl_TessLevelOuter[1] = 2.0;
 gl_TessLevelOuter[2] = 2.0;
 gl_TessLevelOuter[3] = 2.0;

 gl_TessLevelInner[0] = 2.0;
 gl_TessLevelInner[1] = 2.0;
 
 if(gl_in.length() == 3 && 3== gl_InvocationID){
	// I think we're getting triangles. But we wanna output quads.
	// Uhm. Let's just be crude and put the last point in between two other points.
	// TODO: Make the extra point go on the longest line.
	gl_out[gl_InvocationID].gl_Position = 0.5*gl_in[1].gl_Position + 0.5*gl_in[2].gl_Position;
 } else {
	
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
 }
}