
layout(location = 0) out vec3 albedoData;
layout(location = 1) out vec4 colortex7Out;
//layout(location = 2) out vec2 colortex3Out;

/* DRAWBUFFERS:67 */

//uniform mat4 gbufferModelView;

flat in vec4 tint;
in vec2 lightmap;

#include "/lib/Head/Common.inc"

//#define EMISSIVE_SELECT_OUTLINE

float bayer2 (vec2 a) { a = 0.5 * floor(a); return fract(1.5 * fract(a.y) + a.x); }
#define bayer4(a) (bayer2(0.5 * (a)) * 0.25 + bayer2(a))

void main() {
	int materialIDs = 1;

	if (tint.a < 0.1) { discard; return; }

	albedoData = tint.rgb;
	#ifdef EMISSIVE_SELECT_OUTLINE
		if (abs(tint.a - 0.4) + dotSelf(tint.rgb) < 1e-2) {
			materialIDs = 20;
			albedoData = vec3(1.0);
		}
	#endif
	colortex7Out.xy = lightmap + (bayer4(gl_FragCoord.xy) - 0.5) * rcp(255.0);
	colortex7Out.z = float(materialIDs + 0.1) * rcp(255.0);
	//colortex3Out = EncodeNormal(mat3(gbufferModelView) * vec3(0.0, 1.0, 0.0));
}
