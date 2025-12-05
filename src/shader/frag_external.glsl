#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision mediump float;
in vec2 v_texCoord;
layout(location = 0) out vec4 outColor;
uniform samplerExternalOES s_texture;

uniform float time;

void main()
{
  vec4 texColor = texture(s_texture, v_texCoord);
  outColor = texColor;
}
