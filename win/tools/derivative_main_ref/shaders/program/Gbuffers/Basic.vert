
uniform vec2 taaOffset;

flat out vec4 tint;
out vec2 lightmap;

#include "/Settings.glsl"

void main() {
	gl_Position = ftransform();

	#ifdef TAA_ENABLED
		gl_Position.xy += taaOffset * gl_Position.w;
	#endif

	tint = gl_Color;

	lightmap = clamp(gl_MultiTexCoord1.xy * (1.0 / 240.0), 0.0, 1.0);
}