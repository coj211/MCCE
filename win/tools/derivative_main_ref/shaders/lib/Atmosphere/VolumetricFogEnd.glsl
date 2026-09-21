
//#define RAY_STAINED_GLASS_TINT

#define UW_VOLUMETRIC_LIGHT_STRENGTH 0.1 // [0.01 0.015 0.02 0.03 0.05 0.075 0.1 0.15 0.2 0.3 0.5 0.75 1.0 1.5 2.0 3.0 5.0 7.5 10.0 15.0 20.0 30.0 50.0 75.0 100.0]
#define UW_VOLUMETRIC_LIGHT_LENGTH 50.0 // [10.0 15.0 20.0 25.0 30.0 35.0 40.0 45.0 50.0 55.0 60.0 80.0 100.0 120.0 150 200.0 300.0]


//------------------------------------------------------------------------------------------------//

#include "/lib/Lighting/ShadowDistortion.glsl"

vec3 WorldPosToShadowPos(in vec3 worldPos) {
	vec3 shadowClipPos = transMAD(shadowModelView, worldPos);
	shadowClipPos = projMAD(shadowProjection, shadowClipPos);

	return shadowClipPos;
}

#ifdef DARK_END
	float darkEnd = float(bossBattle == 2 || bossBattle == 3);
#else
	const float darkEnd = 0.0;
#endif

vec3 fogWind = vec3(1.0, 0.3, 0.6) * frameTimeCounter * (0.01 + darkEnd * 0.02);

float CalculateFogDensity(in vec3 rayPosition) {
	float falloff = exp2(-abs(rayPosition.y) * 0.02);

	rayPosition *= 0.015;
	rayPosition += fogWind;
	float noise = Get3DNoiseSmooth(rayPosition) * 0.5;
		rayPosition += fogWind;
	noise += Get3DNoiseSmooth(rayPosition * 3.6) * 0.25;
		rayPosition += fogWind;
	noise += Get3DNoiseSmooth(rayPosition * 12.0) * 0.125;
		rayPosition += fogWind;
	noise += Get3DNoiseSmooth(rayPosition * 56.0) * 0.0625;

	//noise = curve(smoothstep(0.2, 0.7, noise)) - falloff * 0.2;

	return saturate(falloff * noise * 40.0 - 8.0 + darkEnd) * 0.2;
}

const int shadowMapResolution = 2048;  // Shadowmap resolution [1024 2048 4096 8192 16384 32768]
const float realShadowMapRes = shadowMapResolution * MC_SHADOW_QUALITY;

vec4 CalculateVolumetricFog(in vec3 worldPos, in vec3 worldDir, in float dither) {	
	//worldPos *= min(1.0, far / length(worldPos));
	//worldPos *= min(1.0, far * 1.2 / length(worldPos));
	//worldPos *= min(1.0, far / length(worldPos));

	#if defined DISTANT_HORIZONS
		#define far float(dhRenderDistance)
	#endif

	float rayLength = min(max(8e2, far), length(worldPos));

	uint steps = uint(12.0 + rayLength * 0.1);
	#if defined DISTANT_HORIZONS
		// steps += uint(dhFarPlane * 0.002);
	    steps = min(steps, 90u);
	#else
	    steps = min(steps, 26u);
	#endif

	float rSteps = 1.0 / float(steps);

	float stepLength = rayLength * rSteps,
		  transmittance = 1.0,
		  LdotV = dot(worldLightVector, worldDir),
		  LdotV01 = LdotV * 0.5 + 0.5,
		  skylightSample = 0.0;

	float airDensity = RayleighPhase(LdotV) * 0.1 / far;

	vec3 rayStep = worldDir * stepLength,
		 rayPosition = rayStep * dither + gbufferModelViewInverse[3].xyz + cameraPosition;

	vec3 shadowStart = WorldPosToShadowPos(gbufferModelViewInverse[3].xyz),
		 shadowEnd = WorldPosToShadowPos(rayStep + gbufferModelViewInverse[3].xyz);

	vec3 shadowStep = shadowEnd - shadowStart,
		 shadowPosition = shadowStep * dither + shadowStart;
	vec3 sunlightSample = vec3(0.0);

	float phases1 = (HenyeyGreensteinPhase(LdotV, 0.5) 		   + HenyeyGreensteinPhase(LdotV, -0.3))		 * 0.5,
		  phases2 = (HenyeyGreensteinPhase(LdotV * 0.5, 0.5)   + HenyeyGreensteinPhase(LdotV * 0.5, -0.3))   * 0.25,
		  phases3 = (HenyeyGreensteinPhase(LdotV * 0.25, 0.5)  + HenyeyGreensteinPhase(LdotV * 0.25, -0.3))  * 0.125,
		  phases4 = (HenyeyGreensteinPhase(LdotV * 0.125, 0.5) + HenyeyGreensteinPhase(LdotV * 0.125, -0.3)) * 0.0625;

	// for (uint i = 0u; i < steps; ++i, rayPosition += rayStep, shadowPosition += shadowStep) {
    uint i = 0u;
	while (++i < steps) {
		rayPosition += rayStep, shadowPosition += shadowStep;

        if (rayPosition.y > 384.0) continue;
		vec3 shadowProjPos = DistortShadowSpace(shadowPosition) * 0.5 + 0.5;
		// if (saturate(shadowProjPos) != shadowProjPos) continue;
		ivec2 shadowTexel = ivec2(shadowProjPos.xy * realShadowMapRes);
	
		float fogDensity = airDensity;
		float density = CalculateFogDensity(rayPosition);
		fogDensity += density;

		if (fogDensity < 1e-5) continue;
        fogDensity *= stepLength;

		vec3 shadow = vec3(1.0);
    	if (saturate(shadowProjPos) == shadowProjPos) {
			shadow = step(shadowProjPos.z, vec3(texelFetch(shadowtex1, shadowTexel, 0).x));

			#ifdef RAY_STAINED_GLASS_TINT
				float translucentShadow = step(shadowProjPos.z, texelFetch(shadowtex0, shadowTexel, 0).x);
				if (shadow.x != translucentShadow) {
					vec3 shadowColorSample = pow4(texelFetch(shadowcolor0, shadowTexel, 0).rgb);
					shadow = shadowColorSample * (shadow - translucentShadow) + vec3(translucentShadow);
				}
			#endif
		}

		if (density > 1e-5) {
			float stepSize = 5.0, sunlightOD = 0.0;
			vec3 checkPos = rayPosition;
			for (uint i = 0u; i < 4u; ++i, checkPos += worldLightVector * stepSize) {
				float density = CalculateFogDensity(checkPos);
				if (density < 1e-5) continue;
				sunlightOD += density * stepSize;
				stepSize *= 1.5;
			}

			float scatteringSun = oneMinus(fastExp(-sunlightOD * 3.0)) * oneMinus(LdotV01) + LdotV01;
			scatteringSun *= fastExp(-sunlightOD * 4.0) * phases1
						+ fastExp(-sunlightOD * 2.0)  	* phases2
						+ fastExp(-sunlightOD * 1.0)  	* phases3
						+ fastExp(-sunlightOD * 0.5) 	* phases4;

			shadow *= (scatteringSun + airDensity) * 4.0;
		}

		float stepTransmittance = fastExp(-fogDensity);

		float powder = 1.0 - fastExp(-fogDensity * 3.0);
		powder = powder * oneMinus(LdotV01) + LdotV01;
		float fogSample = powder * transmittance * oneMinus(stepTransmittance);
		sunlightSample += shadow * fogSample;
		skylightSample += fogSample;

		transmittance *= stepTransmittance;

		if (transmittance < 1e-3) break;
	}

	vec3 fogSunColor = mix(vec3(0.99, 0.88, 0.27), vec3(0.04, 0.02, 0.05), darkEnd * 0.97) * sunlightSample;
	vec3 fogSkyColor = mix(vec3(0.99, 0.95, 0.6), vec3(0.04, 0.03, 0.07), darkEnd * 0.98) * skylightSample;

	vec3 fogColor = fogSunColor * 18.0 + fogSkyColor * 0.08;

	return vec4(fogColor, transmittance);
}

vec3 UnderwaterVolumetricLight(in vec3 worldPos, in vec3 worldDir, in float dither) {
	float rayLength = min(24.0, length(worldPos));

	uint steps = uint(12.0 + 0.5 * rayLength);
	     steps = min(steps, 22u);

	float rSteps = 1.0 / float(steps);

	float stepLength = rayLength * rSteps;

	vec3 shadowStart = WorldPosToShadowPos(gbufferModelViewInverse[3].xyz),
		 shadowEnd = WorldPosToShadowPos(worldDir * stepLength + gbufferModelViewInverse[3].xyz);

	vec3 shadowStep = shadowEnd - shadowStart,
		 shadowPosition = shadowStep * dither + shadowStart;

	const vec3 coeff = waterAbsorption + 0.02;
	vec3 stepTransmittance = fastExp(-coeff * stepLength);
	vec3 transmittance = vec3(1.0);

	vec3 scattering = vec3(0.0);

    uint i = 0u;
	while (++i < steps) {
		shadowPosition += shadowStep;

		vec3 shadowProjPos = DistortShadowSpace(shadowPosition) * 0.5 + 0.5;
		if (saturate(shadowProjPos) != shadowProjPos) continue;
		ivec2 shadowTexel = ivec2(shadowProjPos.xy * realShadowMapRes);
	
        float translucentShadow = step(shadowProjPos.z, texelFetch(shadowtex0, shadowTexel, 0).x);
        vec3 sampleShadow = vec3(1.0);

		if (translucentShadow < 1.0) {
			sampleShadow = step(shadowProjPos.z, texelFetch(shadowtex1, shadowTexel, 0).xxx);

            if (sampleShadow.x != translucentShadow) {
				float waterDepth = abs(texelFetch(shadowcolor1, shadowTexel, 0).w * 512.0 - 128.0 - shadowPosition.y - eyeAltitude);
				if (waterDepth > 0.1) {
					sampleShadow = sqr(cube(texelFetch(shadowcolor0, shadowTexel, 0).rgb));
				} else {
					vec3 shadowColorSample = pow4(texelFetch(shadowcolor0, shadowTexel, 0).rgb);
					sampleShadow = shadowColorSample * (sampleShadow - translucentShadow) + vec3(translucentShadow);
				}

				sampleShadow *= fastExp(-coeff * 0.4 * max(waterDepth, 8.0));
			}
		}

		scattering += sampleShadow * transmittance * oneMinus(stepTransmittance);

		transmittance *= stepTransmittance;
	}

	vec3 lightVector = refract(worldLightVector, vec3(0.0, -1.0, 0.0), 1.0 / WATER_REFRACT_IOR);
	float LdotV = dot(lightVector, worldDir);
	float phase = HenyeyGreensteinPhase(LdotV, 0.8) + HenyeyGreensteinPhase(LdotV, 0.6);

	vec3 fogColor = 8.0 / coeff// * oneMinus(stepTransmittance)
	;
	fogColor *= scattering * phase * UW_VOLUMETRIC_LIGHT_STRENGTH;

	return fogColor;
}