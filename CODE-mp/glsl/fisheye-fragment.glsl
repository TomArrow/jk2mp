#version 400 compatibility

uniform sampler2D text_in;

in vec3 debugColor;
in vec4 vertColor;
in vec3 texUVTransform[2];

in mat4x4 projectionMatrix;

uniform int fishEyeModeUniform; //1= fisheye, 2=equirectangular

varying vec4 eyeSpaceCoordsGeom;

void main(void)
{
    const float depth = 2.0f;

    if(fishEyeModeUniform == 0){
	
		vec2 uvCoords;
		//uvCoords.s = dot(eyeSpaceCoordsGeom.xyz,texUVTransform[0]);
		//uvCoords.t = dot(eyeSpaceCoordsGeom.xyz,texUVTransform[1]);
		vec4 color = texture2D(text_in, gl_TexCoord[0].st);
		//vec4 color = texture2D(text_in, uvCoords);
		float offset = 1.0f - (color.x + color.y + color.z)/3.0f;

		vec3 transposedCoords = eyeSpaceCoordsGeom.xyz + normalize(eyeSpaceCoordsGeom.xyz)*depth * offset;
		
		uvCoords.s = dot(transposedCoords,texUVTransform[0]);
		uvCoords.t = dot(transposedCoords,texUVTransform[1]);
		color = texture2D(text_in, uvCoords);

		gl_FragColor = color*vertColor; 
		gl_FragColor.xyz+=debugColor;
		//gl_FragColor.xyz+=eyeSpaceCoordsGeom.xyz/1000.0f; // cool effect lol
	} else {
		
		vec4 color = texture2D(text_in, gl_TexCoord[0].st);
		gl_FragColor = color*vertColor; 
		gl_FragColor.xyz+=debugColor;
	}
}