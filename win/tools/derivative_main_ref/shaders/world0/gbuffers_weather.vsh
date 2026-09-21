#version 450 compatibility

out float tint;
out vec2 texcoord;

uniform float frameTimeCounter;
uniform vec3 cameraPosition;
uniform mat4 gbufferModelView;
uniform mat4 gbufferModelViewInverse;

uniform vec2 taaOffset;

void main() {
    tint = gl_Color.a;
 	texcoord = mat2(gl_TextureMatrix[0]) * gl_MultiTexCoord0.xy + gl_TextureMatrix[0][3].xy;

	vec4 worldPos = gbufferModelViewInverse * gl_ModelViewMatrix * gl_Vertex;

    float windPos = dot(worldPos.xyz + cameraPosition, vec3(2.0));
    float wind = fma(sin(windPos + frameTimeCounter * 0.1), 0.25, 0.25);
	const float windAngle = radians(180.0) / 60.0;

    worldPos.xz -= worldPos.y * wind * vec2(cos(windAngle), sin(windAngle));
    gl_Position = gl_ProjectionMatrix * gbufferModelView * worldPos;

    #ifdef TAA_ENABLED
        gl_Position.xy += taaOffset * gl_Position.w;
    #endif
}