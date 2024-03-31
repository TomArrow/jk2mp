#version 400 compatibility

uniform sampler2D text_in;

in vec3 debugColor;
in vec4 vertColor;
in vec3 texUVTransform[2];

in mat4x4 projectionMatrix;

in vec3 normal;

uniform int fishEyeModeUniform; //1= fisheye, 2=equirectangular
uniform float texAverageBrightnessUniform;
uniform float parallaxMapDepthUniform;
uniform int parallaxMapLayersUniform;
uniform float parallaxMapGammaUniform;
uniform int isLightmapUniform; // Not currently filled

varying vec4 eyeSpaceCoordsGeom;


vec2 parallaxMap(){
		vec2 uvCoords;
		//uvCoords.s = dot(eyeSpaceCoordsGeom.xyz,texUVTransform[0]);
		//uvCoords.t = dot(eyeSpaceCoordsGeom.xyz,texUVTransform[1]);
		vec4 color = texture2D(text_in, gl_TexCoord[0].st);
		//vec4 color = texture2D(text_in, uvCoords);
		float offset = 1.0f - max(min((color.x + color.y + color.z)/3.0f/texAverageBrightnessUniform,1.0f),0.0f);

		vec3 offset3d =  normalize(eyeSpaceCoordsGeom.xyz)*parallaxMapDepthUniform * offset;
		offset3d -= normal * dot(normal,offset3d); // project onto surface aka get rid of any 3d component that aligns with the normal of the surface

		vec3 transposedCoords = eyeSpaceCoordsGeom.xyz + offset3d;
		
		uvCoords.s = mod(dot(transposedCoords,texUVTransform[0]),1);
		uvCoords.t = mod(dot(transposedCoords,texUVTransform[1]),1);
		return uvCoords;
}
vec2 parallaxMapSteep(){
		int layers = parallaxMapLayersUniform;
		vec2 uvCoords;

		float layerDepth = parallaxMapDepthUniform / float(layers);
		vec3 currentPlace = eyeSpaceCoordsGeom.xyz;
		float gamma = 1.0f/parallaxMapGammaUniform;

		//vec4 color = texture2D(text_in, gl_TexCoord[0].st);

		vec3 viewVecNormalized = normalize(eyeSpaceCoordsGeom.xyz);
		vec3 depthComponent = normal * dot(normal,viewVecNormalized); // Get the depth component that a unity view vector gives us 
		vec3 viewVecFlat = viewVecNormalized - depthComponent;
		float viewVecMultiplier = layerDepth/length(depthComponent); // Calculate how much we have to multiple the unity view vector with to go one layer deeper.
		vec3 oneLayerProgressVec = viewVecFlat*viewVecMultiplier;

		uvCoords.s = mod(dot(currentPlace,texUVTransform[0]),1);
		uvCoords.t = mod(dot(currentPlace,texUVTransform[1]),1);

		for(int i=0; i< layers;i++){
			
			vec4 color = texture2D(text_in, uvCoords);
			float texDepth = parallaxMapDepthUniform*(pow(max(min((color.x + color.y + color.z)/3.0f/texAverageBrightnessUniform,1.0f),0.0f),gamma)-1.0f);
			if(texDepth >= -(layerDepth * float(i))){
				break;
			} else {
				currentPlace += oneLayerProgressVec;
				uvCoords.s = mod(dot(currentPlace,texUVTransform[0]),1);
				uvCoords.t = mod(dot(currentPlace,texUVTransform[1]),1);
			}
		}

		//float offset = 1.0f - max(min((color.x + color.y + color.z)/3.0f/texAverageBrightnessUniform,1.0f),0.0f);


		//float offset = 1.0f - max(min((color.x + color.y + color.z)/3.0f/texAverageBrightnessUniform,1.0f),0.0f);

		//vec3 offset3d =  normalize(eyeSpaceCoordsGeom.xyz)*parallaxMapDepthUniform * offset;
		//float depthHere = dot(normal,offset3d);
		//vec3 depthComponent = normal * depthHere;
		//offset3d -= depthComponent; // project onto surface aka get rid of any 3d component that aligns with the normal of the surface

		//vec3 transposedCoords = eyeSpaceCoordsGeom.xyz + offset3d;
		
		return uvCoords;
}

void main(void)
{
    //const float depth = 5.0f;

    if(fishEyeModeUniform == 0){
	
		vec2 uvCoords = parallaxMapLayersUniform < 2 ? parallaxMap():parallaxMapSteep();
		vec4 color = texture2D(text_in, uvCoords);

		gl_FragColor = color*vertColor; 
		gl_FragColor.xyz+=debugColor;
		//gl_FragColor.xyz+=eyeSpaceCoordsGeom.xyz/1000.0f; // cool effect lol
	} else {
		
		vec4 color = texture2D(text_in, gl_TexCoord[0].st);
		gl_FragColor = color*vertColor; 
		gl_FragColor.xyz+=debugColor;
	}
}