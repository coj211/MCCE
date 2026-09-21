
layout(location = 0) out vec3 albedoData;
layout(location = 1) out vec3 colortex7Out;
layout(location = 2) out vec2 colortex3Out;

/* DRAWBUFFERS:673 */

uniform sampler2D tex;

in vec3 flatNormal;

in vec4 tint;
in vec2 texcoord;
in vec2 lightmap;
flat in int materialIDs;

#include "/lib/Head/Common.inc"

float bayer2 (vec2 a) { a = 0.5 * floor(a); return fract(1.5 * fract(a.y) + a.x); }
#define bayer4(a) (bayer2(0.5 * (a)) * 0.25 + bayer2(a))

void main() {
	vec4 albedo = texture(tex, texcoord) * tint;

	if (albedo.r < 0.29 && albedo.g < 0.45 && albedo.b > 0.75 || albedo.a < 0.1) { discard; return; }

	#ifdef WHITE_WORLD
		albedo.rgb = vec3(1.0);
	#endif

	albedoData = albedo.rgb;

	colortex7Out.xy = lightmap + (bayer4(gl_FragCoord.xy) - 0.5) * rcp(255.0);
	colortex7Out.z = float(materialIDs + 0.1) * rcp(255.0);
	colortex3Out = EncodeNormal(flatNormal);
}
