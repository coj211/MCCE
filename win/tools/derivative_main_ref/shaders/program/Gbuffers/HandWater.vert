
uniform vec2 taaOffset;
uniform vec2 screenSize;

#ifndef MC_GL_VENDOR_INTEL
	#define attribute in
#endif

attribute vec4 at_tangent;

out vec4 tint;
out vec2 texcoord;
out vec4 viewPos;

out vec2 lightmap;

flat out mat3 tbnMatrix;

#include "/Settings.glsl"

void main() {
	tint = gl_Color;
	texcoord = mat2(gl_TextureMatrix[0]) * gl_MultiTexCoord0.xy + gl_TextureMatrix[0][3].xy;

	viewPos = gl_ModelViewMatrix * gl_Vertex;
	gl_Position = gl_ProjectionMatrix * viewPos;

    tbnMatrix[2] = normalize(gl_NormalMatrix * gl_Normal);
	#ifdef MC_NORMAL_MAP
		tbnMatrix[0] = normalize(gl_NormalMatrix * at_tangent.xyz);
		tbnMatrix[1] = cross(tbnMatrix[0], tbnMatrix[2]) * sign(at_tangent.w);
	#endif

	#ifdef TAA_ENABLED
		gl_Position.xy += taaOffset * gl_Position.w;
	#endif

	lightmap = clamp(gl_MultiTexCoord1.xy * (1.0 / 240.0), 0.0, 1.0);
}