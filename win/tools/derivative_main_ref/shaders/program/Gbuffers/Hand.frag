
layout(location = 0) out vec4 albedoData;
layout(location = 1) out vec3 colortex7Out;
layout(location = 2) out vec4 colortex3Out;

/* DRAWBUFFERS:673 */

#include "/lib/Head/Common.inc"

uniform sampler2D tex;
#ifdef MC_NORMAL_MAP
    uniform sampler2D normals;
#endif
#ifdef MC_SPECULAR_MAP
    uniform sampler2D specular;
#endif

uniform float wetnessCustom;

in vec4 tint;
in vec2 texcoord;
in vec4 viewPos;

in vec2 lightmap;

flat in mat3 tbnMatrix;

//#include "/lib/Surface/ManualTBN.glsl"

float bayer2 (vec2 a) { a = 0.5 * floor(a); return fract(1.5 * fract(a.y) + a.x); }
#define bayer4(a) (bayer2(0.5 * (a)) * 0.25 + bayer2(a))

void main() {	
	vec4 albedo = texture(tex, texcoord) * tint;

	#ifdef WHITE_WORLD
		albedo.rgb = vec3(1.0);
	#endif

    //mat3 tbnMatrix = manualTBN(viewPos.xyz, texcoord);

	if (albedo.a < 0.1) { discard; return; }

	#ifdef MC_SPECULAR_MAP
		vec4 specularData = texture(specular, texcoord);
	#else
		vec4 specularData = vec4(0.0);
	#endif

    #ifdef MC_NORMAL_MAP
        vec3 normalData = texture(normals, texcoord).rgb;
        DecodeNormalTex(normalData);
    #else
        vec3 normalData = vec3(0.0, 0.0, 1.0);
    #endif

	#if defined IS_OVERWORLD
		if (wetnessCustom > 1e-2) {
			float wetFact = wetnessCustom;
			wetFact *= remap(0.5, 0.9, tbnMatrix[2].y);
			wetFact *= saturate(lightmap.y * 10.0 - 9.0);

			#ifdef MC_NORMAL_MAP
				normalData = mix(normalData.xyz, vec3(0.0, 0.0, 1.0), wetFact);
			#endif

			#ifdef FORCE_WET_EFFECT
				specularData.r = mix(specularData.r, 1.0, wetFact);
				specularData.g = max(specularData.g, 0.04 * wetFact);
			#endif

			vec3 wetAlbedo = ColorSaturation(albedo.rgb, 0.75) * 0.85;
			#ifdef POROSITY
				float porosity = specularData.b > 64.5 / 255.0 ? 0.0 : remap(specularData.b, 0.0, 64.0 / 255.0) * 0.7;
				wetAlbedo *= oneMinus(porosity) / oneMinus(porosity * wetAlbedo);
			#endif
			albedo.rgb = mix(albedo.rgb, wetAlbedo, wetFact);
		}
	#endif

	normalData = normalize(tbnMatrix * normalData);

	albedoData = albedo;

	colortex7Out.xy = lightmap + (bayer4(gl_FragCoord.xy) - 0.5) * rcp(255.0);
	colortex7Out.z = 4.1 / 255.0;

	colortex3Out.xy = EncodeNormal(normalData);
	colortex3Out.z = PackUnorm2x8(specularData.rg);
	colortex3Out.w = PackUnorm2x8(specularData.ba);
}
