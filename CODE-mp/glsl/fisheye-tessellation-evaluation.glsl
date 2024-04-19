#version 400 compatibility
//#extension GL_ARB_tessellation_shader : enable

//layout(quads, equal_spacing, ccw) in;
layout(triangles, equal_spacing, ccw) in;

out vec3 debugColor;
out vec4 color;
out vec4 geomTexCoord;
in vec4 vertexTexCoord[];
in vec4 colorTCS[];

in mat4x4 worldModelViewMatrixReverseTCS[];
out mat4x4 worldModelViewMatrixReverse;

in mat4x4 projectionMatrixTCS[];
out mat4x4 projectionMatrix;

in vec3 normalTCS[];
out vec3 normal;

in vec4 eyeSpaceCoordsTCS[];
out vec4 eyeSpaceCoords;

in vec4 pureVertexCoordsTCS[];
out vec4 pureVertexCoords;

uniform int fishEyeModeUniform; //1= fisheye, 2=equirectangular

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

  
  //worldModelViewMatrixReverse = (gl_TessCoord.x * worldModelViewMatrixReverseTCS[0] + gl_TessCoord.y * worldModelViewMatrixReverseTCS[1] + gl_TessCoord.z * worldModelViewMatrixReverseTCS[2]);
  worldModelViewMatrixReverse = worldModelViewMatrixReverseTCS[0];
  projectionMatrix = projectionMatrixTCS[0];
  normal = (gl_TessCoord.x * normalTCS[0] + gl_TessCoord.y * normalTCS[1] + gl_TessCoord.z * normalTCS[2]);
  eyeSpaceCoords = (gl_TessCoord.x * eyeSpaceCoordsTCS[0] + gl_TessCoord.y * eyeSpaceCoordsTCS[1] + gl_TessCoord.z * eyeSpaceCoordsTCS[2]);
  pureVertexCoords = (gl_TessCoord.x * pureVertexCoordsTCS[0] + gl_TessCoord.y * pureVertexCoordsTCS[1] + gl_TessCoord.z * pureVertexCoordsTCS[2]);

}