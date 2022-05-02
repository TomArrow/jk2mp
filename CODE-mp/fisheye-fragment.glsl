
uniform sampler2D text_in;

in vec3 color;

void main(void)
{
	gl_FragColor = texture2D(text_in, gl_TexCoord[0].st); 
	//gl_FragColor.xyz+=color;
}