uniform vec3 originUniform;
uniform vec3 axisUniform[3];

out vec3 color;
out float realDepth;

float angleOnPlane(vec3 point, vec3 axis1, vec3 axis2){
	
	vec2 planePosition = vec2( dot(point,axis1), dot(point,axis2));
	return acos(dot(vec2(1,0),normalize(planePosition)));
}

vec3 getPerpendicularAxis(vec3 point, vec3 mainAxis){
	return point - dot(point,mainAxis)*mainAxis;
}

void debugColor(float x,float y, float z){
	//color = vec3(max(0.0,min(1.0,x)),max(0.0,min(1.0,y)),max(0.0,min(1.0,z)));
}

void equirectangular(){


  float pi =radians(180);

  vec3 axis[3] = axisUniform;
  axis[0] = vec3(0.0,0.0,-1.0);
  axis[1] = vec3(-1.0,0.0,0.0);
  axis[2] = vec3(0.0,1.0,0.0);

  vec4 correctPos = gl_ModelViewMatrix * gl_Vertex;

  debugColor(correctPos[0]*0.01,correctPos[1]*0.01,-correctPos[2]*0.0001);

  vec3 pointVec = -correctPos.xyz;

  vec4 test;
  float distance = length(pointVec);
  pointVec = normalize(pointVec);
  float depth = dot(axis[0].xyz,pointVec);
  float height = dot(axis[2].xyz,pointVec);
  float heightSign = sign(height);
  float width = dot(-axis[1].xyz,pointVec);
  float widthSign = sign(width);

  realDepth = depth;
  
  debugColor(width,height,depth*0.01);
  
  float xAngle = angleOnPlane(pointVec,-axis[1].xyz,axis[0].xyz)/pi;
  
  vec3 perpendicularAxis = getPerpendicularAxis(pointVec,axis[2].xyz);
  float yAngle = angleOnPlane(pointVec,axis[2].xyz,perpendicularAxis)/pi;
    
  xAngle -= 0.5;
  xAngle *= 2;
  widthSign = sign(xAngle);
  xAngle = depth<=0 ? xAngle : widthSign*(1.0+(1.0-abs(xAngle)));

  yAngle -= 0.5;
  yAngle *= 2;
  heightSign = sign(yAngle);
  
  debugColor(xAngle,yAngle,depth*0.01);

  xAngle *= 0.5;

  test.x = xAngle;
  test.y = yAngle;
  test.z = 1.0-1.0/distance;
  test.w= 1.0;
  gl_Position = test;

  gl_TexCoord[0] = gl_MultiTexCoord0;
}


void main(void)
{   
	equirectangular();
	
}
