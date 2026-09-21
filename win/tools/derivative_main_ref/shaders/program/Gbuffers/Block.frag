
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

uniform float frameTimeCounter;

uniform mat4 gbufferModelViewInverse;

in vec4 tint;
in vec2 texcoord;
in vec3 minecraftPos;
in vec4 viewPos;

in vec2 lightmap;

//flat in mat3 tbnMatrix;

flat in int materialIDs;

#define PROGRAM_GBUFFERS_BLOCK

#ifndef RAIN_SPLASH_EFFECT
	#undef PROGRAM_GBUFFERS_BLOCK
#endif

#include "/lib/Surface/ManualTBN.glsl"

const vec3[] COLORS = vec3[](
    vec3(0.022087, 0.098399, 0.110818),
    vec3(0.011892, 0.095924, 0.089485),
    vec3(0.027636, 0.101689, 0.100326),
    vec3(0.046564, 0.109883, 0.114838),
    vec3(0.064901, 0.117696, 0.097189),
    vec3(0.063761, 0.086895, 0.123646),
    vec3(0.084817, 0.111994, 0.166380),
    vec3(0.097489, 0.154120, 0.091064),
    vec3(0.106152, 0.131144, 0.195191),
    vec3(0.097721, 0.110188, 0.187229),
    vec3(0.133516, 0.138278, 0.148582),
    vec3(0.070006, 0.243332, 0.235792),
    vec3(0.196766, 0.142899, 0.214696),
    vec3(0.047281, 0.315338, 0.321970),
    vec3(0.204675, 0.390010, 0.302066),
    vec3(0.080955, 0.314821, 0.661491)
);

mat2 mat2RotateZ(in float radian) {
	return mat2(
		cos(radian), -sin(radian),
		sin(radian), cos(radian)
	);
}

vec2 endPortalLayer(in vec2 coord, in float layer) {
	vec2 offset = vec2(8.5 / layer, (1.0 + layer / 3.0) * (frameTimeCounter * 0.0015)) + 0.25;

	mat2 rotate = mat2RotateZ(radians(layer * layer * 8642.0 + layer * 18.0));

	return (4.5 - layer / 4.0) * (rotate * coord) + offset;
}

#if defined IS_OVERWORLD
	uniform sampler2D noisetex;
	uniform sampler2D colortex7;

	uniform float wetnessCustom;

	#include "/lib/Surface/RainEffect.glsl"
#endif

float bayer2 (vec2 a) { a = 0.5 * floor(a); return fract(1.5 * fract(a.y) + a.x); }
#define bayer4(a) (bayer2(0.5 * (a)) * 0.25 + bayer2(a))

void main() {
	vec4 albedo = texture(tex, texcoord) * tint;

	#ifdef WHITE_WORLD
		albedo.rgb = vec3(1.0);
	#endif

    mat3 tbnMatrix = manualTBN(viewPos.xyz, texcoord);

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
    		float noise = GetRainWetness(minecraftPos.xz - minecraftPos.y);
			noise *= remap(0.5, 0.9, (mat3(gbufferModelViewInverse) * tbnMatrix[2]).y);
			noise *= saturate(lightmap.y * 10.0 - 9.0);
			//noise *= wetnessCustom;
    		float wetFact = smoothstep(0.54, 0.62, noise);

			#ifdef RAIN_SPLASH_EFFECT
				normalData = mix(normalData.xyz, vec3(GetRainNormal(minecraftPos), 1.0), wetFact * 0.5);
			#else
				normalData = mix(normalData.xyz, vec3(0.0, 0.0, 1.0), wetFact);
			#endif

    		wetFact = sqr(remap(0.35, 0.57, noise));

			#ifdef FORCE_WET_EFFECT
				specularData.r = mix(specularData.r, 1.0, wetFact);
				specularData.g = max(specularData.g, 0.04 * wetFact);
				specularData.rg += (bayer4(gl_FragCoord.xy) - 0.5) * rcp(255.0);
			#endif

			vec3 wetAlbedo = ColorSaturation(albedo.rgb, 0.75) * 0.85;
			#ifdef POROSITY
				float porosity = specularData.b > 64.5 / 255.0 ? 0.0 : remap(specularData.b, 0.0, 64.0 / 255.0) * 0.7;
				wetAlbedo *= oneMinus(porosity) / oneMinus(porosity * wetAlbedo);
			#endif
			albedo.rgb = mix(albedo.rgb, wetAlbedo, sqr(remap(0.3, 0.56, noise)));
		}
	#endif

	normalData = normalize(tbnMatrix * normalData);

	if (materialIDs == 19) {
		vec3 worldDir = mat3(gbufferModelViewInverse) * normalize(viewPos.xyz);
		vec3 worldDirAbs = abs(worldDir);
		vec3 samplePartAbs = step(maxOf(worldDirAbs), worldDirAbs);
		vec3 samplePart = samplePartAbs * sign(worldDir);
		float intersection = 1.0 / dot(samplePartAbs, worldDirAbs);
		vec3 sampleNDCRaw = samplePart - worldDir * intersection;
		vec2 sampleNDC = sampleNDCRaw.xy * vec2(samplePartAbs.y + samplePart.z, 1.0 - samplePartAbs.y) + sampleNDCRaw.z * vec2(-samplePart.x, samplePartAbs.y);
		vec2 portalCoord = sampleNDC * 0.5 + 0.5;

		vec3 portalColor = texture(tex, portalCoord).rgb * COLORS[0];
		for (int i = 0; i < 16; ++i) {
			portalColor += texture(tex, endPortalLayer(portalCoord, float(i + 1))).rgb * COLORS[i];
		}
		albedo.rgb = portalColor;
		specularData = vec4(1.0, 0.04, vec2(254.0 / 255.0));
	}

	#if TEXTURE_FORMAT == 0 && defined MC_SPECULAR_MAP
		#if SUBSERFACE_SCATTERING_MODE == 1
			if (materialIDs == 9) specularData.b = max(0.65, specularData.b);
		#elif SUBSERFACE_SCATTERING_MODE == 0
			if (materialIDs == 9) specularData.b = 0.65;
		#endif
	#elif SUBSERFACE_SCATTERING_MODE < 2
		specularData.a = 0.0;
		if (materialIDs == 9) specularData.a = 0.65;
	#endif


	albedoData = albedo;

	colortex7Out.xy = lightmap + (bayer4(gl_FragCoord.xy) - 0.5) * rcp(255.0);
	colortex7Out.z = float(materialIDs + 0.1) * rcp(255.0);

	colortex3Out.xy = EncodeNormal(normalData);
	colortex3Out.z = PackUnorm2x8(specularData.rg);
	colortex3Out.w = PackUnorm2x8(specularData.ba);
}
