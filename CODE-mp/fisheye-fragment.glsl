
uniform sampler2D text_in;

in vec3 debugColor;
in vec4 vertColor;

void main(void)
{
	gl_FragColor = texture2D(text_in, gl_TexCoord[0].st)*vertColor; 
	//gl_FragColor.xyz+=color;
}