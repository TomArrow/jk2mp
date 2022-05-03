uniform vec3 dofJitterUniform;
uniform float dofFocusUniform;
uniform float dofRadiusUniform;

out vec3 debugColor;
out vec4 color;
out float realDepth;

float angleOnPlane(vec3 point, vec3 axis1, vec3 axis2){
	
	vec2 planePosition = vec2( dot(point,axis1), dot(point,axis2));
	return acos(dot(vec2(1,0),normalize(planePosition)));
}

vec3 getPerpendicularAxis(vec3 point, vec3 mainAxis){
	return point - dot(point,mainAxis)*mainAxis;
}

void setDebugColor(float x,float y, float z){
	//debugColor = vec3(max(0.0,min(1.0,x)),max(0.0,min(1.0,y)),max(0.0,min(1.0,z)));
}


float calcDofFactor(float pointDistance, float dofFocus){
	return abs(pointDistance-dofFocus);// /pointDistance;
}

void equirectangular(){

  float pi =radians(180);

  vec3 axis[3];
  axis[0] = vec3(0.0,0.0,-1.0);
  axis[1] = vec3(-1.0,0.0,0.0);
  axis[2] = vec3(0.0,1.0,0.0);

  vec4 correctPos = gl_ModelViewMatrix * gl_Vertex;
  vec3 pointVec = -correctPos.xyz;

  float distance = length(pointVec);

  vec3 perpendicularZAxisToPoint = getPerpendicularAxis(normalize(pointVec),axis[2].xyz);

  // Apply DOF jitter
  float dofFactor = calcDofFactor(distance,dofFocusUniform)*dofRadiusUniform*0.001; // 0.001 to get it roughly in line with normal dof in mme. 
  vec3 perpendicularXAxisToPoint = cross( axis[2].xyz,perpendicularZAxisToPoint);
  pointVec.x += dofFactor*dot(dofJitterUniform,perpendicularXAxisToPoint);
  pointVec.y += dofFactor*dot(dofJitterUniform,axis[2].xyz);
  pointVec.z += dofFactor*dot(dofJitterUniform,perpendicularZAxisToPoint);


  pointVec = normalize(pointVec);

  vec4 outputPos;

  float depth = dot(axis[0].xyz,pointVec);
  realDepth = depth;
  
  float xAngle = angleOnPlane(pointVec,-axis[1].xyz,axis[0].xyz)/pi;
  
  float yAngle = angleOnPlane(pointVec,axis[2].xyz,perpendicularZAxisToPoint)/pi;
    
  xAngle -= 0.5;
  xAngle *= 2;
  float widthSign = sign(xAngle);
  xAngle = depth<=0 ? xAngle : widthSign*(1.0+(1.0-abs(xAngle)));
  xAngle *= 0.5;

  yAngle -= 0.5;
  yAngle *= 2;
  float heightSign = sign(yAngle);

  outputPos.x = xAngle;
  outputPos.y = yAngle;
  outputPos.z = 1.0-1.0/distance;
  outputPos.w= 1.0;
  gl_Position = outputPos;

  gl_TexCoord[0] = gl_MultiTexCoord0;

  color = gl_Color;
}


void main(void)
{   
	equirectangular();
	
}
