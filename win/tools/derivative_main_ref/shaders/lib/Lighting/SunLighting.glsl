
#include "/lib/Surface/BRDF.glsl"

#define PCF_SAMPLES 16 // [4 6 8 10 12 14 16 18 20 22 24 26 28 30 32 48 64]

const float shadowDistanceRenderMul = 1.0; // [-1.0 1.0]

const int shadowMapResolution = 2048;  // Shadowmap resolution [1024 2048 4096 8192 16384 32768]
const float	shadowDistance	  = 192.0; // [64.0 80.0 96.0 112.0 128.0 160.0 192.0 224.0 256.0 320.0 384.0 512.0 768.0 1024.0 2048.0 4096.0 8192.0 16384.0 32768.0 65536.0]

const float realShadowMapRes = shadowMapResolution * MC_SHADOW_QUALITY;


//------------------------------------------------------------------------------------------------//

#include "ShadowDistortion.glsl"

vec3 WorldPosToShadowProjPosBias(in vec3 worldOffsetPos, out float distortFactor) {
	vec3 shadowClipPos = transMAD(shadowModelView, worldOffsetPos);
	shadowClipPos = projMAD(shadowProjection, shadowClipPos);

	distortFactor = DistortionFactor(shadowClipPos.xy);
	return DistortShadowSpace(shadowClipPos, distortFactor) * 0.5 + 0.5;
}

//------------------------------------------------------------------------------------------------//

vec2 BlockerSearch(in vec3 shadowProjPos, in float dither) {
	float searchDepth = 0.0;
	float sumWeight = 0.0;
	float sssDepth = 0.0;

	float searchRadius = 2.0 * shadowProjection[0].x;

	vec2 rot = cossin(dither * TAU) * searchRadius;
	const vec2 angleStep = cossin(TAU * 0.125);
	const mat2 rotStep = mat2(angleStep, -angleStep.y, angleStep.x);
	for (uint i = 0u; i < 8u; ++i, rot *= rotStep) {
		float fi = float(i) + dither;
		vec2 sampleCoord = shadowProjPos.xy + rot * sqrt(fi * 0.125);

		float depthSample = texelFetch(shadowtex0, ivec2(sampleCoord * realShadowMapRes), 0).x;
		float weight = step(depthSample, shadowProjPos.z);

		sssDepth += max0(shadowProjPos.z - depthSample);
		searchDepth += depthSample * weight;
		sumWeight += weight;
	}

	searchDepth *= 1.0 / sumWeight;
	searchDepth = min(2.0 * (shadowProjPos.z - searchDepth) / searchDepth, 1.0);

	return vec2(searchDepth * shadowProjection[0].x, sssDepth * shadowProjectionInverse[2].z);
}

vec3 PercentageCloserFilter(in vec3 shadowProjPos, in float dither, in float penumbraScale) {
	shadowProjPos.z -= 1e-4 - dither * 5e-5;

	// const uint steps = 16u;
	const float rSteps = 1.0 / float(PCF_SAMPLES);

	vec3 result = vec3(0.0);

	vec2 rot = cossin(dither * TAU) * penumbraScale;
	const vec2 angleStep = cossin(TAU * 0.125);
	const mat2 rotStep = mat2(angleStep, -angleStep.y, angleStep.x);
	for (uint i = 0u; i < PCF_SAMPLES; ++i, rot *= rotStep) {
		float fi = float(i) + dither;
		vec2 sampleCoord = shadowProjPos.xy + rot * sqrt(fi * rSteps);

		float sampleDepth1 = textureLod(shadowtex1, vec3(sampleCoord, shadowProjPos.z), 0).x;

	#ifdef COLORED_SHADOWS
		ivec2 sampleTexel = ivec2(sampleCoord * realShadowMapRes);
		float sampleDepth0 = step(shadowProjPos.z, texelFetch(shadowtex0, sampleTexel, 0).x);
		if (sampleDepth0 != sampleDepth1) {
			result += pow4(texelFetch(shadowcolor0, sampleTexel, 0).rgb) * sampleDepth1;
		} else 
	#endif
		{ result += sampleDepth1; }
	}

	return result * rSteps;
}

//------------------------------------------------------------------------------------------------//

float ScreenSpaceShadow(in vec3 viewPos, in vec3 rayPos, in float dither, in float sssAmount) {
	vec3 lightVector = mat3(gbufferModelView) * worldLightVector;

    vec3 position = ViewToScreenSpace(lightVector * abs(viewPos.z) * 0.1 + viewPos);
    vec3 screenDir = normalize(position - rayPos);

	float absorption = pow(sssAmount, sqrt(length(viewPos)) * 0.5);

    screenDir.xy *= screenSize;
    rayPos.xy *= screenSize;

    vec3 rayStep = screenDir * max(0.01, 0.05 - sssAmount * 0.05) * gbufferProjection[1][1] * rcp(12.0);
    rayPos += rayStep * (1.0 - sssAmount + dither);

	const float zTolerance = 0.025;
	float shadow = 1.0;

    for (uint i = 0u; i < 12u; ++i, rayPos += rayStep) {
        if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) break;
        if (rayPos.z >= 1.0) break;

        float depth = texelFetch(depthtex0, ivec2(rayPos.xy), 0).x;

        if (depth < rayPos.z) {
			float linearSample = GetDepthLinear(depth);
			float currentDepth = GetDepthLinear(rayPos.z);

			if (abs(linearSample - currentDepth) / currentDepth < zTolerance) {
				shadow *= absorption;
				// break;
			}
        }

		if (shadow < 1e-2) break;
    }

	return shadow;
}

#if defined DISTANT_HORIZONS
	float ScreenSpaceShadowDH(in vec3 viewPos, in vec3 rayPos, in float dither, in float sssAmount) {
		vec3 lightVector = mat3(gbufferModelView) * worldLightVector;

		vec3 position = ViewToScreenSpaceDH(lightVector * abs(viewPos.z) * 0.1 + viewPos);
		vec3 screenDir = normalize(position - rayPos);

		float absorption = pow(sssAmount, sqrt(length(viewPos)) * 0.5);

		screenDir.xy *= screenSize;
		rayPos.xy *= screenSize;

		vec3 rayStep = screenDir * max(0.01, 0.05 - sssAmount * 0.05) * dhProjection[1][1] * rcp(12.0);
		rayPos += rayStep * (1.0 - sssAmount + dither);

		const float zTolerance = 0.025;
		float shadow = 1.0;

		for (uint i = 0u; i < 12u; ++i, rayPos += rayStep) {
			if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) break;
			// if (rayPos.z >= 1.0) break;

			float depth = texelFetch(dhDepthTex0, ivec2(rayPos.xy), 0).x;

			if (depth < rayPos.z) {
				float linearSample = GetDepthLinearDH(depth);
				float currentDepth = GetDepthLinearDH(rayPos.z);

				if (abs(linearSample - currentDepth) / currentDepth < zTolerance) {
					shadow *= absorption;
					// break;
				}
			}

			if (shadow < 1e-2) break;
		}

		return shadow;
	}
#endif

float CalculateFakeBouncedLight(in vec3 normal) {
	normal.y = -normal.y;
	vec3 bounceVector = normalize(worldLightVector + vec3(0.0, 1.0, 0.0));
	float bounce = saturate(dot(normal, bounceVector) * 0.4 + 0.6);

	return bounce * (2.0 - bounce) * 3e-2;
}

vec3 CalculateSubsurfaceScattering(in vec3 albedo, in float sssAmount, in float sssDepth, in float LdotV) {
	//if (sssAmount < 1e-4) return vec3(0.0);

	vec3 coeff = albedo * inversesqrt(GetLuminance(albedo) + 1e-6);
	coeff = oneMinus(0.75 * saturate(coeff)) * (28.0 / sssAmount);

	vec3 subsurfaceScattering =  fastExp(0.375 * coeff * sssDepth) * HenyeyGreensteinPhase(-LdotV, 0.6);
		 subsurfaceScattering += fastExp(0.125 * coeff * sssDepth) * (0.33 * HenyeyGreensteinPhase(-LdotV, 0.35) + 0.17 * rPI);

	//vec3 subsurfaceScattering = fastExp(coeff * sssDepth);
	//subsurfaceScattering *= HenyeyGreensteinPhase(-LdotV, 0.65) + 0.25;
	return subsurfaceScattering * sssAmount * PI;
}
