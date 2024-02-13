#version 300 es                                   
precision mediump float;                          
in vec2 v_texCoord;                               
layout(location = 0) out vec4 outColor;           
uniform sampler2D s_texture;

uniform float time;
void main()                                       
{                                                 
  //outColor = texture2D(s_texture, v_texCoord.xy);
  outColor = vec4(sin(time), cos(time), tan(time), 1);  
}                                                  