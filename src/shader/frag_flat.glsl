#version 300 es                                   
precision mediump float;                          
in vec2 v_texCoord;                               
layout(location = 0) out vec4 outColor;

in float vertexID;

uniform float time;

void main()                                       
{

  if (fract(vertexID) < 0.1){
      outColor = vec4(1,0,0,0);
  }
  outColor = vec4(sin(time), cos(time), tan(time), 1);
}