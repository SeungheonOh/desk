#version 300 es
precision mediump float;

in vec2 v_uv;
in vec2 v_screen_pos;

uniform vec2 u_resolution;
uniform vec2 u_center;
uniform sampler2D u_screen_texture;

out vec4 out_color;

void main() {
    vec2 uv = v_uv;
    float dist = length(uv);

    // Discard points outside circle
    if (dist > 1.0) discard;

    // Convex lens distortion - spherical bulging effect
    float sphereProfile = sqrt(1.0 - dist * dist);
    float distortionStrength = (1.0 - sphereProfile) * 2.5;

    // Push sample position inward toward center
    vec2 sampleLocalPos = uv * (1.0 - distortionStrength);

    // Clamp to circle bounds
    float sampleDist = length(sampleLocalPos);
    if (sampleDist > 1.0) {
        sampleLocalPos = normalize(sampleLocalPos) * 0.99;
    }

    // Convert to screen UV
    float radius = 28.0;
    vec2 sampleScreenPos = v_screen_pos + sampleLocalPos * radius;
    vec2 screenUV = sampleScreenPos / u_resolution;

    // Chromatic aberration - sample RGB at slightly different offsets
    float chromaticOffset = 0.008 * (1.0 - sphereProfile);
    vec2 offsetDir = normalize(uv + vec2(0.001));
    
    float r = texture(u_screen_texture, screenUV + offsetDir * chromaticOffset).r;
    float g = texture(u_screen_texture, screenUV).g;
    float b = texture(u_screen_texture, screenUV - offsetDir * chromaticOffset).b;
    
    vec3 refractedColor = vec3(r, g, b);

    // Brightness boost in center (lens focus)
    float centerBoost = 1.0 + 0.15 * sphereProfile;
    refractedColor *= centerBoost;

    // Edge darkening (fresnel-like)
    float edgeDarken = mix(0.7, 1.0, sphereProfile);
    refractedColor *= edgeDarken;

    // Subtle edge glow
    float edgeGlow = smoothstep(0.85, 1.0, dist) * 0.4;
    vec3 glowColor = vec3(0.6, 0.8, 1.0);
    refractedColor = mix(refractedColor, glowColor, edgeGlow);

    // Specular highlight
    vec2 lightDir = normalize(vec2(-0.5, -0.7));
    float specular = pow(max(0.0, dot(normalize(uv), lightDir)), 8.0);
    specular *= sphereProfile * 0.5;
    refractedColor += vec3(1.0) * specular;

    // Anti-aliased soft edge using screen-space derivatives
    float edgeWidth = fwidth(dist) * 1.5;
    float alpha = smoothstep(1.0, 1.0 - edgeWidth, dist);
    
    out_color = vec4(refractedColor, alpha);
}
