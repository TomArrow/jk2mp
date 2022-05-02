// Geometry Shader
  #extension GL_ARB_geometry_shader4 : enable

  in float realDepth[3];

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

    bool emit = true;
    if(someInBack && someLeft && someRight){
        emit = false;
    }

    if(emit){
        for(int i = 0; i < 3; i++)
        {
          gl_Position = gl_PositionIn[i];
          gl_TexCoord[0] = gl_TexCoordIn[i][0];
          EmitVertex();
        }
    }
    

  }