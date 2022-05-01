uniform vec3 originUniform;
uniform vec3 axisUniform[3];

void main(void)
{   
  vec4 normaltransform = ftransform();

  float pi =radians(180);

  //gl_Position = ftransform(gl_Vertex);
  //gl_Position = transform( gl_ModelViewProjectionMatrix, gl_Vertex );
  //gl_Position.x *= (1-(abs(gl_Position.y)/540.0))*0.5+0.5;
  //gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

  //vec4 test = gl_Vertex;
  //test.xyz += axisUniform[0].xyz*100;
  //gl_Position = gl_ModelViewProjectionMatrix * test;
  vec3 pointVec = originUniform-gl_Vertex.xyz;
  vec4 test;
  //test.x = dot(axisUniform[1].xyz,pointVec);
  //test.y = dot(-axisUniform[0].xyz,pointVec);
  //test.z = dot(axisUniform[2].xyz,pointVec);

  // Kinda iso perspective:
 // test.x = dot(axisUniform[1].xyz,pointVec);
  //test.y = dot(-axisUniform[2].xyz,pointVec);
  //test.z = dot(-axisUniform[0].xyz,pointVec);
  //test.xyz *= 0.005;
  //test.z *= 0.001;

  pointVec = normalize(pointVec);
  float depth = dot(axisUniform[0].xyz,pointVec);
  float height = dot(axisUniform[2].xyz,pointVec);
  float heightSign = sign(height);

  // Kinda iso perspective:
  //test.x = acos(dot(axisUniform[1].xyz,pointVec));
  test.x = normaltransform.x;
  //test.x = (acos(dot(axisUniform[1].xyz,pointVec))/pi*normaltransform.w-normaltransform.w/2)*4;
  test.y = depth <0 ? (acos(height)/pi*normaltransform.w-normaltransform.w/2)*4 : heightSign*normaltransform.w;
  //test.y = normaltransform.y*0.5;
  test.z = normaltransform.z;
  test.w= normaltransform.w;
  gl_Position = test;

  gl_TexCoord[0] = gl_MultiTexCoord0;
}
