uniform vec3 originUniform;
uniform vec3 axisUniform[3];

void main(void)
{   
  //gl_Position = ftransform(gl_Vertex);
  //gl_Position = transform( gl_ModelViewProjectionMatrix, gl_Vertex );
  //gl_Position.x *= (1-(abs(gl_Position.y)/540.0))*0.5+0.5;
  //gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  vec4 test = gl_Vertex;
  test.xyz += axisUniform[0].xyz*100;
  gl_Position = gl_ModelViewProjectionMatrix * test;
  gl_TexCoord[0] = gl_MultiTexCoord0;
}
