#version 400 compatibility
//#extension GL_ARB_tessellation_shader : enable

//layout(quads, equal_spacing, ccw) in;
layout(triangles, equal_spacing, ccw) in;

out vec4 color;
out vec4 geomTexCoord;
in vec4 vertexTexCoord[];
in vec4 colorTCS[];

//quad interpol
vec4 interpolate(in vec4 v0, in vec4 v1, in vec4 v2, in vec4 v3)
{
 vec4 a = mix(v0, v1, gl_TessCoord.x);
 vec4 b = mix(v3, v2, gl_TessCoord.x);
 return mix(a, b, gl_TessCoord.y);
}

void main()
{ 
 //gl_Position = interpolate(
 // gl_in[0].gl_Position, 
 // gl_in[1].gl_Position, 
  //gl_in[2].gl_Position, 
 // gl_in[3].gl_Position);
  gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position + gl_TessCoord.z * gl_in[2].gl_Position);
  geomTexCoord = (gl_TessCoord.x * vertexTexCoord[0] + gl_TessCoord.y * vertexTexCoord[1] + gl_TessCoord.z * vertexTexCoord[2]);
  color = (gl_TessCoord.x * colorTCS[0] + gl_TessCoord.y * colorTCS[1] + gl_TessCoord.z * colorTCS[2]);
  //color = vec4(1,1,1,1);
  //geomTexCoord = teUv;
  //geomTexCoord = vertexTexCoord;
}