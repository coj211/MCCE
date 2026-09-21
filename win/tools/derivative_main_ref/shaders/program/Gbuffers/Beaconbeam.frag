
#include "/lib/Head/Common.inc"

layout(location = 0) out vec3 albedoData;
layout(location = 1) out vec3 colortex7Out;
#ifdef MC_SPECULAR_MAP
	layout(location = 2) out vec4 colortex3Out;
#else
	layout(location = 2) out vec2 colortex3Out;
#endif

/* RENDERTARGETS:6,7,3 */

uniform sampler2D tex;
#ifdef MC_SPECULAR_MAP
    uniform sampler2D specular;
#endif

in vec3 flatNormal;

in vec4 tint;
in vec2 texcoord;
//in vec2 lightmap;

// float bayer2 (vec2 a) { a = 0.5 * floor(a); return fract(1.5 * fract(a.y) + a.x); }
// #define bayer4(a) (bayer2(0.5 * (a)) * 0.25 + bayer2(a))

void main() {
	vec4 albedo = texture(tex, texcoord) * tint;

	// if (albedo.a < 0.999) { discard; return; }

	#ifdef WHITE_WORLD
		albedo.rgb = vec3(1.0);
	#endif

	albedoData = albedo.rgb;

	//colortex7Out.xy = lightmap + (bayer4(gl_FragCoord.xy) - 0.5) * rcp(255.0);
	colortex7Out.xy = vec2(1.0);
	colortex7Out.z = 36.1 / 255.0;
	colortex3Out.xy = EncodeNormal(flatNormal);
	#ifdef MC_SPECULAR_MAP
		vec4 specularData = texture(specular, texcoord);
		colortex3Out.z = PackUnorm2x8(specularData.rg);
		colortex3Out.w = PackUnorm2x8(specularData.ba);
	#endif
}
