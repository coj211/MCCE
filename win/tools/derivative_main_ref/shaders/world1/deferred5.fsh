#version 450 compatibility


#define IS_END

layout(location = 0) out vec2 specularData;
layout(location = 1) out vec3 sceneData;

uniform sampler2DShadow shadowtex1;
uniform sampler2D shadowtex0;
uniform sampler2D shadowcolor0;
uniform sampler2D shadowcolor1;

in vec2 screenCoord;

//const vec3 sunIlluminance = vec3(0.2);
//const vec3 skyIlluminance = vec3(0.1);

flat in vec3 blocklightColor;

//flat in vec4 skySHR;
//flat in vec4 skySHG;
//flat in vec4 skySHB;

//in vec3 worldLightVector;
//in vec3 worldSunVector;

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

#ifdef DARK_END
	bool darkEnd = bossBattle == 2 || bossBattle == 3;
#else
	const bool darkEnd = false;
#endif

//----// STRUCTS //-------------------------------------------------------------------------------//

#include "/lib/Head/Mask.inc"
#include "/lib/Head/Material.inc"

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

float HenyeyGreensteinPhase(in float cosTheta, in const float g) { // Henyey Greenstein
  	const float gg = sqr(g);
    float phase = 1.0 + gg - 2.0 * g * cosTheta;
    return oneMinus(gg) / (4.0 * PI * phase * (phase * 0.5 + 0.5));
}

#include "/lib/Lighting/SunLighting.glsl"

//#include "/lib/Water/WaterFog.glsl"

#ifdef GI_ENABLED
	vec4 SpatialFilter(in vec3 normal, in float dist, in float NdotV) {
		ivec2 texel = ivec2(gl_FragCoord.xy) / 2;

		float sumWeight = 0.1;
		vec4 light = texelFetch(colortex0, texel, 0) * sumWeight;

		for (uint i = 0u; i < 16u; ++i) {
			ivec2 offset = offset4x4[i];
			ivec2 sampleTexel = texel + offset * 2;
			if (clamp(sampleTexel, ivec2(1), ivec2(screenSize * 0.5) - 1) != sampleTexel) continue;

			vec4 prevData = texelFetch(colortex0, sampleTexel + ivec2(viewWidth * 0.5, 0), 0);

			float weight = exp2(-dotSelf(offset) * 0.1);
			weight *= exp2(-distance(prevData.w, dist) * 4.0 * NdotV); // Dist
			weight *= pow16(max0(dot(prevData.xyz, normal))); // Normal

			light += texelFetch(colortex0, sampleTexel, 0) * weight;
			sumWeight += weight;
		}

		light /= max(1e-6, sumWeight);
		//light.rgb = SRGBtoLinear(light.rgb);

		return light;
	}
#endif

#define coneAngleToSolidAngle(x) (TAU * oneMinus(cos(x)))

float fastAcos(float x) {
    float a = abs(x);
	float r = 1.570796 - 0.175394 * a;
	r *= sqrt(1.0 - a);

	return x < 0.0 ? PI - r : r;
}

vec3 RenderSun(in vec3 worldDir, in vec3 sunVector) {
	//const float sunRadius = 1392082.56;
	//const float sunDist = 149.6e6;

	//const float sunAngularRadius = sunRadius / sunDist * 0.5;
	const float sunAngularRadius = TAU / 360.0;

	//const vec3 sunIlluminance = vec3(1.0, 0.973, 0.961) * 126.6e3;
	const vec3 sunIlluminance = vec3(1.0, 0.973, 0.961) * 0.2;
	const vec3 sunLuminance = sunIlluminance / coneAngleToSolidAngle(sunAngularRadius);

    float cosTheta = dot(worldDir, sunVector);
    float centerToEdge = saturate(fastAcos(cosTheta) / sunAngularRadius);
    float cosSunRadius = cos(sunAngularRadius);
    if (cosTheta < cosSunRadius) return vec3(0.0);

	const vec3 alpha = vec3(0.429, 0.522, 0.614); // for AP1 primaries

    vec3 factor = pow(vec3(1.0 - centerToEdge * centerToEdge), alpha * 0.5);
    vec3 finalLuminance = sunLuminance * factor;

	float visibility = curve(saturate(worldDir.y));

    return finalLuminance * visibility;
}

vec3 RenderStars(in vec3 worldDir) {
	const float scale = 288.0;
	const float coverage = 0.02;
	const float maxLuminance = 5.0;
	const int minTemperature = 4000;
	const int maxTemperature = 8000;

	float visibility = oneMinus(exp2(-max0(worldDir.y) * 2.0));

	float cosine = worldSunVector.z;
	vec3 axis = cross(worldSunVector, vec3(0, 0, 1));
	float cosecantSquared = rcp(dotSelf(axis));
	worldDir = cosine * worldDir + cross(axis, worldDir) + (cosecantSquared - cosecantSquared * cosine) * dot(axis, worldDir) * axis;

	vec3  p = worldDir * scale;
	ivec3 i = ivec3(floor(p));
	vec3  f = p - i;
	float r = dotSelf(f - 0.5);

	vec3 i3 = fract(i * vec3(443.897, 441.423, 437.195));
	i3 += dot(i3, i3.yzx + 19.19);
	vec2 hash = fract((i3.xx + i3.yz) * i3.zy);
	hash.y = 2.0 * hash.y - 4.0 * hash.y * hash.y + 3.0 * hash.y * hash.y * hash.y;

	float cov = smoothstep(oneMinus(coverage), 1.0, hash.x);
	return visibility * maxLuminance * smoothstep(0.25, 0.0, r) * cov * cov * Blackbody(mix(minTemperature, maxTemperature, hash.y));
}

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);

	//float skyLightmapT = texelFetch(colortex2, texel, 0).g;
	//GetSkylightFalloff(skyLightmapT);

	//vec3 normal = GetNormals(texel);
	//TranslucentMask materialMaskT = CalculateMasksT(materialIDT);

	float depth = GetDepthFix(texel);
	//depth += 0.38 * step(depth, 0.56);
	//float depthT = GetDepthFix(texel);

	//if (materialMask.particle || materialMask.particleGlowing) depth = depthT;

	vec3 viewPos = ScreenToViewSpace(vec3(screenCoord, depth));

	#if defined DISTANT_HORIZONS
		bool dhRange = depth >= 1.0;
		if (dhRange) {
			depth = GetDepthDH(texel);
			viewPos = ScreenToViewSpaceDH(vec3(screenCoord, depth));
		}
	#endif

	vec3 worldPos = mat3(gbufferModelViewInverse) * viewPos;
	vec3 worldDir = normalize(worldPos);
	// worldPos += gbufferModelViewInverse[3].xyz;

	vec4 gbuffer7 = texelFetch(colortex7, texel, 0);

	int materialID = int(gbuffer7.z * 255.0);

	if (depth >= 1.0 && materialID != 36) {
		//Sky
		sceneData = mix(vec3(0.396, 0.352, 0.108), vec3(0.04, 0.025, 0.045), float(darkEnd) * 0.96) * exp2(-max0(worldDir.y) * 1.5);

		if (!darkEnd) {
			vec3 sunDisc = RenderSun(worldDir, worldSunVector);
			sunDisc *= vec3(0.99, 0.93, 0.65);
			sceneData += sunDisc + RenderStars(worldDir);
		}

		specularData = vec2(1.0, 0.0);
	} else {
		sceneData = vec3(0.0);

		vec3 albedoRaw = texelFetch(colortex6, texel, 0).rgb;
		vec3 albedo = SRGBtoLinear(albedoRaw);

		vec4 gbuffer3 = texelFetch(colortex3, texel, 0);

		//int materialIDT = int(texelFetch(colortex1, texel, 0).b * 255.0);

		//MaterialMask materialMask = CalculateMasks(materialID);
		bool isGrass = materialID == 6 || materialID == 27 || materialID == 28 || materialID == 33;

		vec2 mcLightmap = gbuffer7.rg;
		//mcLightmap.g = cube(mcLightmap.g);

		vec3 normal = DecodeNormal(gbuffer3.xy);
		vec3 worldNormal = mat3(gbufferModelViewInverse) * normal;

		vec4 specTex = vec4(UnpackUnorm2x8(gbuffer3.z), UnpackUnorm2x8(gbuffer3.w));
		Material material = GetMaterialData(specTex);
		specTex.x = sqr(1.0 - specTex.x);
		specularData = specTex.xy;

		float rawNdotL = dot(worldNormal, worldLightVector);

		// Grass points up
		if (isGrass) worldNormal = vec3(0.0, 1.0, 0.0);

		float opaqueDepth = -viewPos.z;
		//float waterDepth = ScreenToViewSpaceDepth(depthT);
		float LdotV = dot(worldLightVector, -worldDir);

		//vec3 halfWay = normalize(worldLightVector - worldDir);
		float NdotV = saturate(dot(worldNormal, -worldDir));
		float NdotL = max0(dot(worldNormal, worldLightVector));
		float halfwayNorm = inversesqrt(2.0 * LdotV + 2.0);
		float NdotH = (NdotL + NdotV) * halfwayNorm;
		float LdotH = LdotV * halfwayNorm + halfwayNorm;

		// Sunlight
		vec3 waterTint = isEyeInWater == 1 ? vec3(0.6, 0.9, 1.2) / max(3.0, opaqueDepth * 0.1 * WATER_FOG_DENSITY) : vec3(1.0);	
		vec3 sunlightMult = darkEnd ? vec3(0.04, 0.03, 0.06) : vec3(3.0, 2.7, 2.0);
		sunlightMult *= waterTint;
		vec3 diffuse = vec3(1.0);

		#ifdef TAA_ENABLED
			float dither = BlueNoiseTemporal();
		#else
			float dither = InterleavedGradientNoise(gl_FragCoord.xy);
		#endif

		//float sssDepth;
		worldPos += gbufferModelViewInverse[3].xyz;

		float distortFactor;
		// vec3 normalOffset = worldNormal * min(0.25, dotSelf(worldPos) * 6e-5 + 3e-2) * (2.0 - saturate(NdotL));
		vec3 normalOffset = worldNormal * (dotSelf(worldPos) * 8e-5 + 3e-2) * (2.0 - saturate(NdotL));

		vec3 shadowProjPos = WorldPosToShadowProjPosBias(worldPos + normalOffset, distortFactor);	

		float distanceFade = saturate(pow16(rcp(shadowDistance * shadowDistance) * dotSelf(worldPos)));

		vec2 blockerSearch = BlockerSearch(shadowProjPos, dither);

		if (materialID == 35 || materialID == 36) specTex.ba += 0.2;

		#if TEXTURE_FORMAT == 0 && defined MC_SPECULAR_MAP
			float hasSSScattering = step(64.5 / 255.0, specTex.b);
			float sssAmount = oneMinus(distanceFade) * remap(64.0 / 255.0, 1.0, specTex.b * hasSSScattering) * SUBSERFACE_SCATTERING_STRENTGH;
		#else
			float sssAmount = oneMinus(distanceFade) * remap(64.0 / 255.0, 1.0, specTex.a) * SUBSERFACE_SCATTERING_STRENTGH;
		#endif
		if (sssAmount > 1e-4) {
			vec3 subsurfaceScattering = CalculateSubsurfaceScattering(albedo, sssAmount, blockerSearch.y, LdotV);
			sunlightMult *= 1.0 - sssAmount * 0.5;
			sceneData += subsurfaceScattering * sunlightMult;
		}

		vec3 shadow = vec3(0.0);
		vec3 specular = vec3(0.0);
		if (NdotL > 1e-3) {
			float penumbraScale = max(blockerSearch.x / distortFactor, 2.0 / realShadowMapRes);
			shadow = PercentageCloserFilter(shadowProjPos, dither, penumbraScale);

			if (maxOf(shadow) > 1e-6) {
				#ifdef SCREEN_SPACE_SHADOWS
					#if defined DISTANT_HORIZONS
						if (dhRange) shadow *= ScreenSpaceShadowDH(viewPos, vec3(screenCoord, depth), dither, sssAmount);		
						else shadow *= ScreenSpaceShadow(viewPos, vec3(screenCoord, depth), dither, sssAmount);
					#else
						shadow *= ScreenSpaceShadow(viewPos, vec3(screenCoord, depth), dither, sssAmount);
					#endif
				#endif
				diffuse *= DiffuseHammon(LdotV, NdotV, NdotL, NdotH, material.roughness, albedo);

				#ifdef PARALLAX_SHADOW
					shadow *= oneMinus(gbuffer7.a);
				#endif

				specular = SpecularBRDF(LdotH, NdotV, rawNdotL, NdotH, sqr(material.roughness), material.f0) * mix(vec3(1.0), albedo, material.isMetal);
				specular *= SPECULAR_HIGHLIGHT_BRIGHTNESS;

				// if (isEyeInWater == 0) shadow *= sign(mcLightmap.g);
				shadow *= sunlightMult;
			}
		}

		// Basic light
		sceneData += BASIC_BRIGHTNESS_END + nightVision;
		if (darkEnd) sceneData *= vec3(0.2, 0.1, 0.3);

		// GI AO
		#ifdef GI_ENABLED
			vec4 indirectData = SpatialFilter(normal, opaqueDepth, NdotV);
			float ao = indirectData.a;
		#elif defined SSAO_ENABLED
			float ao = texelFetch(colortex0, texel / 2, 0).a;
		#else
			float ao = 1.0;
		#endif

		#if defined GI_ENABLED
			if (distanceFade > 1e-3) indirectData.rgb = indirectData.rgb * oneMinus(distanceFade) + 0.04 * oneMinus(saturate(NdotL * 1e2)) * distanceFade;
			//if (isEyeInWater == 0) indirectData.rgb *= saturate(mcLightmap.g * 5.0);
			sceneData += indirectData.rgb * GI_BRIGHTNESS * sunlightMult * 0.25;
		#else
			float bounce = CalculateFakeBouncedLight(worldNormal);
			//if (isEyeInWater == 0) bounce *= saturate(mcLightmap.g * 2.0);
			sceneData += bounce * sunlightMult;
		#endif

		sceneData *= ao;

		// Block light
		#include "/lib/Lighting/BlockLighting.glsl"

		sceneData += shadow * diffuse;
		sceneData *= albedo;

		//material.isMetal *= 0.8;
		sceneData *= oneMinus(material.isMetal * 0.8);
		sceneData += shadow * specular;
	}

	sceneData = clamp16F(sceneData * 2.0);
}

/* DRAWBUFFERS:04 */