
layout(location = 0) out vec3 albedoData;
layout(location = 2) out vec4 colortex3Out;
layout(location = 1) out vec3 colortex7Out;

/* DRAWBUFFERS:673 */

#include "/lib/Head/Common.inc"


in vec3 tint;

in vec2 lightmap;

in vec3 flatNormal;
in vec3 worldPos;

flat in uint materialIDs;

uniform int frameCounter;
uniform float far;

float InterleavedGradientNoiseTemporal(in vec2 coord) {
    return fract(52.9829189 * fract(0.06711056 * coord.x + 0.00583715 * coord.y + 0.00623715 * (frameCounter & 63)));
}

void main() {
    if (length(worldPos) < 0.8 * far) { discard; return; }

	vec3 albedo = tint;

	float dither = InterleavedGradientNoiseTemporal(gl_FragCoord.xy);

	#ifdef WHITE_WORLD
		albedo = vec3(1.0);
	#endif

	albedoData = albedo;

	colortex7Out.xy = lightmap + (dither - 0.5) * rcp(255.0);
	colortex7Out.z = float(materialIDs + 0.1) * rcp(255.0);

	vec4 specularData = vec4(0.0);

	// #if defined IS_OVERWORLD
	// 	if (wetnessCustom > 1e-2) {
    // 		float noise = GetRainWetness(minecraftPos.xz - minecraftPos.y);
	// 		noise *= remap(0.5, 0.9, (mat3(gbufferModelViewInverse) * tbnMatrix[2]).y);
	// 		noise *= saturate(lightmap.y * 10.0 - 9.0);
	// 		//noise *= wetnessCustom;
    // 		float wetFact = smoothstep(0.54, 0.62, noise);

	// 		#ifdef RAIN_SPLASH_EFFECT
	// 			normalData = mix(normalData.xyz, vec3(GetRainNormal(minecraftPos), 1.0), wetFact * 0.5);
	// 		#else
	// 			normalData = mix(normalData.xyz, vec3(0.0, 0.0, 1.0), wetFact);
	// 		#endif

    // 		wetFact = sqr(remap(0.35, 0.57, noise));

	// 		#ifdef FORCE_WET_EFFECT
	// 			specularData.r = mix(specularData.r, 1.0, wetFact);
	// 			specularData.g = max(specularData.g, 0.04 * wetFact);
	// 			specularData.rg += (dither - 0.5) * rcp(255.0);
	// 		#endif

	// 		vec3 wetAlbedo = ColorSaturation(albedo.rgb, 0.75) * 0.85;
	// 		#ifdef POROSITY
	// 			float porosity = specularData.b > 64.5 / 255.0 ? 0.0 : remap(specularData.b, 0.0, 64.0 / 255.0) * 0.7;
	// 			wetAlbedo *= oneMinus(porosity) / oneMinus(porosity * wetAlbedo);
	// 		#endif
	// 		albedo.rgb = mix(albedo.rgb, wetAlbedo, sqr(remap(0.3, 0.56, noise)));
	// 	}
	// #endif

	#if TEXTURE_FORMAT == 0
		if (materialIDs == 6u) specularData.b = 0.45;
		if (materialIDs == 7u) specularData.b = 0.7;
	#elif SUBSERFACE_SCATTERING_MODE < 2
		if (materialIDs == 6u) specularData.a = 0.45;
		if (materialIDs == 7u) specularData.a = 0.7;
	#endif

	colortex3Out.xy = EncodeNormal(flatNormal);
	// colortex3Out.z = PackUnorm2x8(specularData.rg);
	colortex3Out.w = PackUnorm2x8(specularData.ba);
}
