
uniform vec2 taaOffset;

#ifndef MC_GL_VENDOR_INTEL
	#define attribute in
#endif

attribute vec4 at_tangent;

out vec4 viewPos;
out vec4 tint;
out vec2 texcoord;

out vec2 lightmap;

flat out mat3 tbnMatrix;

#include "/Settings.glsl"

void main() {
	tint = gl_Color;
	texcoord = gl_MultiTexCoord0.xy;

	lightmap = clamp(gl_MultiTexCoord1.xy * (1.0 / 240.0), 0.0, 1.0);

    tbnMatrix[2] = normalize(gl_NormalMatrix * gl_Normal);
	#ifdef MC_NORMAL_MAP
		tbnMatrix[0] = normalize(gl_NormalMatrix * at_tangent.xyz);
		tbnMatrix[1] = cross(tbnMatrix[0], tbnMatrix[2]) * sign(at_tangent.w);
	#endif

	viewPos = gl_ModelViewMatrix * gl_Vertex;
	gl_Position = gl_ProjectionMatrix * viewPos;

	#ifdef TAA_ENABLED
		gl_Position.xy += taaOffset * gl_Position.w;
	#endif
}