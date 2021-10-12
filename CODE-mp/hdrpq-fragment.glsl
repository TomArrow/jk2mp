
uniform sampler2D text_in;

void main(void)
{
  gl_FragColor = 4*texture2D(text_in, gl_TexCoord[0].st);
}