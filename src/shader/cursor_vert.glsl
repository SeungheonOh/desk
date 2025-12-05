#version 300 es
precision mediump float;

layout(location = 0) in vec2 a_position;

uniform vec2 u_resolution;
uniform vec2 u_center;
uniform float u_radius;

out vec2 v_uv;
out vec2 v_screen_pos;

void main() {
    vec2 local = a_position * u_radius;
    vec2 world = local + u_center;
    v_uv = a_position;
    v_screen_pos = world;
    
    // Convert to NDC: flip Y for position only, keep v_screen_pos in Wayland coords for texture sampling
    vec2 ndc = (world / u_resolution) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, ndc.y, 0.0, 1.0);
}
