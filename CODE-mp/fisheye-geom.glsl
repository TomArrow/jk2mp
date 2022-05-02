// Geometry Shader
  #extension GL_ARB_geometry_shader4 : enable

  in float realDepth[3];
  in vec4 color[3];
  out vec4 vertColor;

  // --------------------
  void main()
  {
    bool someInBack = false;
    bool someLeft = false;
    bool someRight = false;
    for(int i = 0; i < 3; i++)
    {
        if(realDepth[i]>0) someInBack = true;
        if(gl_PositionIn[i].x <= 0) someLeft = true;
        if(gl_PositionIn[i].x > 0) someRight = true;
    }

    bool wrappedAround = false;
    if(someInBack && someLeft && someRight){
        wrappedAround = true;
    }

    if(!wrappedAround){
        for(int i = 0; i < 3; i++)
        {
          gl_Position = gl_PositionIn[i];
          gl_TexCoord[0] = gl_TexCoordIn[i][0];
          vertColor = color[i]; 
          EmitVertex();
        }
            EndPrimitive();
    } else {
        // Emit 2 separate vertices.
        for(int i = 0; i < 3; i++)
        {
            vec4 thisPosition = gl_PositionIn[i];
            if(realDepth[i]>0){
                if(thisPosition.x <= 0) thisPosition.x+=2.0;
            }
            gl_Position = thisPosition;
            gl_TexCoord[0] = gl_TexCoordIn[i][0];
            vertColor = color[i]; 
            EmitVertex();
        }
            EndPrimitive();
        for(int i = 0; i < 3; i++)
        {
            vec4 thisPosition = gl_PositionIn[i];
            if(realDepth[i]>0){
                if(thisPosition.x > 0) thisPosition.x-=2.0;
            }
            gl_Position = thisPosition;
            gl_TexCoord[0] = gl_TexCoordIn[i][0];
            vertColor = color[i]; 
            EmitVertex();
        }
            EndPrimitive();
    }
    

  }