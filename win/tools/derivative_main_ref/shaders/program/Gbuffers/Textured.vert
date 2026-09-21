
uniform vec2 taaOffset;

out vec3 flatNormal;

out vec4 tint;
out vec2 texcoord;
out vec2 lightmap;
flat out int materialIDs;

#include "/Settings.glsl"

void main() {
	texcoord = mat2(gl_TextureMatrix[0]) * gl_MultiTexCoord0.xy + gl_TextureMatrix[0][3].xy;

	lightmap = clamp(gl_MultiTexCoord1.xy * (1.0 / 240.0), 0.0, 1.0);

	gl_Position = ftransform();

	#ifdef TAA_ENABLED
		gl_Position.xy += taaOffset * gl_Position.w;
	#endif

	tint = gl_Color;
	flatNormal = normalize(gl_NormalMatrix * gl_Normal);

	materialIDs = 35;
	if (lightmap.x > 0.99) materialIDs = 36;
}