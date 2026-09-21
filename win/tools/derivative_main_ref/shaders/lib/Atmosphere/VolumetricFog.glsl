
#define VOLUMETRIC_FOG_DENSITY 0.002 // [0.0001 0.0002 0.0005 0.0007 0.001 0.0015 0.002 0.0025 0.003 0.0035 0.004 0.005 0.006 0.007 0.01 0.015 0.02 0.025 0.03 0.035 0.04 0.05 0.07 0.1]
#define SEA_LEVEL 63.0 // [0.0 1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0 11.0 12.0 13.0 14.0 15.0 16.0 17.0 18.0 19.0 20.0 21.0 22.0 23.0 24.0 25.0 26.0 27.0 28.0 29.0 30.0 31.0 32.0 33.0 34.0 35.0 36.0 37.0 38.0 39.0 40.0 41.0 42.0 43.0 44.0 45.0 46.0 47.0 48.0 49.0 50.0 51.0 52.0 53.0 54.0 55.0 56.0 57.0 58.0 59.0 60.0 61.0 62.0 63.0 64.0 65.0 66.0 67.0 68.0 69.0 70.0 71.0 72.0 73.0 74.0 75.0 76.0 77.0 78.0 79.0 80.0 81.0 82.0 83.0 84.0 85.0 86.0 87.0 88.0 89.0 90.0 91.0 92.0 93.0 94.0 95.0 96.0 97.0 98.0 99.0 100.0 101.0 102.0 103.0 104.0 105.0 106.0 107.0 108.0 109.0 110.0 111.0 112.0 113.0 114.0 115.0 116.0 117.0 118.0 119.0 120.0 121.0 122.0 123.0 124.0 125.0 126.0 127.0 128.0 129.0 130.0 131.0 132.0 133.0 134.0 135.0 136.0 137.0 138.0 139.0 140.0 141.0 142.0 143.0 144.0 145.0 146.0 147.0 148.0 149.0 150.0 151.0 152.0 153.0 154.0 155.0 156.0 157.0 158.0 159.0 160.0 161.0 162.0 163.0 164.0 165.0 166.0 167.0 168.0 169.0 170.0 171.0 172.0 173.0 174.0 175.0 176.0 177.0 178.0 179.0 180.0 181.0 182.0 183.0 184.0 185.0 186.0 187.0 188.0 189.0 190.0 191.0 192.0 193.0 194.0 195.0 196.0 197.0 198.0 199.0 200.0 201.0 202.0 203.0 204.0 205.0 206.0 207.0 208.0 209.0 210.0 211.0 212.0 213.0 214.0 215.0 216.0 217.0 218.0 219.0 220.0 221.0 222.0 223.0 224.0 225.0 226.0 227.0 228.0 229.0 230.0 231.0 232.0 233.0 234.0 235.0 236.0 237.0 238.0 239.0 240.0 241.0 242.0 243.0 244.0 245.0 246.0 247.0 248.0 249.0 250.0 251.0 252.0 253.0 254.0 255.0]
#define VOLUMETRIC_FOG_SAMPLES 20 // [2 4 6 8 9 10 12 14 15 16 18 20 24 30 50 70 100 150 200 300 500]

#define VOLUMETRIC_LIGHT_STRENGTH 0.2 // [0.001 0.002 0.005 0.007 0.01 0.02 0.03 0.04 0.05 0.07 0.1 0.12 0.15 0.17 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.7 0.8 0.9 1.0 1.5 2.0 3.0 4.0 5.0 7.0 10.0]
#define UW_VOLUMETRIC_LIGHT_STRENGTH 0.1 // [0.01 0.015 0.02 0.03 0.05 0.075 0.1 0.15 0.2 0.3 0.5 0.75 1.0 1.5 2.0 3.0 5.0 7.5 10.0 15.0 20.0 30.0 50.0 75.0 100.0]
#define UW_VOLUMETRIC_LIGHT_LENGTH 50.0 // [10.0 15.0 20.0 25.0 30.0 35.0 40.0 45.0 50.0 55.0 60.0 80.0 100.0 120.0 150 200.0 300.0]
//#define RAY_STAINED_GLASS_TINT
#define TIME_FADE


//------------------------------------------------------------------------------------------------//

#ifdef CLOUDS_SHADOW
	#include "/lib/Atmosphere/VolumetricClouds.glsl"

	#define CLOUD_PLANE_ALTITUDE 7000 // [400 500 1000 1200 1500 1700 2000 3000 4000 5000 6000 6500 7000 7500 8000 9000 10000 12000]
	#define CLOUD_PLANE1_COVERY 0.5 // [0 0.05 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.65 0.7 0.8 0.9 1.0]

	float CloudPlanarDensity(in vec2 worldPos) {
    	worldPos /= 1.0 + distance(worldPos, cameraPosition.xz) * 2e-5;
		// float localCoverage = texture(noisetex, worldPos * 2e-7 - wind.xz * 2e-3).y;
		// localCoverage = saturate(fma(localCoverage, 3.0, -0.6));
		// if (localCoverage < 0.1) return 0.0;
		//position = rotateWindAngle * position;
		vec2 position = worldPos * 1e-4 - wind.xz;

		float baseCoverage = curve(texture(noisetex, position * 0.08).z * 0.7 + 0.1);
		baseCoverage *= max0(1.07 - texture(noisetex, position * 0.003).y * 1.4);
		//localCoverage = remap(0.32, 0.7, localCoverage)/* + wetness * 0.4*/;
		//if (baseCoverage < 0.1) return 0.0;

		vec2 curl = texture(noisetex, position * 0.05).xy * 0.04;
		curl += texture(noisetex, position * 0.1).xy * 0.02;
		//position *= 2.5;
		//position = rotateWindAngle * 2.5 * position;
		position += curl;
		float noise = 0.5 * texture(noisetex, position * vec2(0.4, 0.16)).z;
		//position = rotateWindAngle * 2.5 * position;
		noise += texture(noisetex, position * 0.9).z - 0.24;
		noise = saturate(noise);

		#ifdef CLOUDS_WEATHER
			noise -= cloudDynamicWeather.x;
		#endif

		noise *= clamp((baseCoverage + CLOUD_PLANE1_COVERY - 0.6) * 0.9, 0.0, 0.14);
    	if (noise < 1e-6) return 0.0;
		//noise += PC_COVERY - 0.7;
		position.x += noise * 0.2;

		noise += 0.02 * texture(noisetex, position * 3.0).z;
		noise += 0.01 * texture(noisetex, position * 5.0 + curl).z - 0.05;
		//noise *= curve(pow(baseCoverage, 0.6) + 0.02);

		return cube(saturate(noise * (4.0 + wetness)));
	}

	float CalculateCloudShadow(in vec3 worldPos, in CloudProperties cloudProperties) {
		float cloudDensity = 0.0;
		vec3 checkOrigin = worldPos + vec3(0.0, planetRadius, 0.0);
		#ifdef VC_SHADOW
			float checkRadius = planetRadius + cloudProperties.altitude;
			vec3 checkPos = RaySphereIntersection(checkOrigin, worldLightVector, checkRadius + 0.15 * cloudProperties.thickness).y * worldLightVector + worldPos;
			// vec3 checkPos = worldLightVector / abs(worldLightVector.y) * max0(cloudProperties.maxAltitude + 30.0 - worldPos.y) + worldPos;
			cloudDensity = CloudVolumeDensitySmooth(cloudProperties, checkPos) * 2.0;
		#endif
		#ifdef PC_SHADOW
			vec2 checkPos1 = RaySphereIntersection(checkOrigin, worldLightVector, planetRadius + CLOUD_PLANE_ALTITUDE).y * worldLightVector.xz + worldPos.xz;
			cloudDensity += CloudPlanarDensity(checkPos1) * 10.0;
		#endif
		// cloudDensity = mix(0.4, cloudDensity, saturate(sqr(abs(worldLightVector.y) * 2.0)));
		// cloudDensity = mix(cloudDensity, 0.9, wetness * 0.5);
		cloudDensity = saturate(cloudDensity);

		return fastExp(cloudDensity * cloudDensity * -2e2);
	}
#endif

#include "/lib/Lighting/ShadowDistortion.glsl"

vec3 WorldPosToShadowPos(in vec3 worldPos) {
	vec3 shadowClipPos = transMAD(shadowModelView, worldPos);
	shadowClipPos = projMAD(shadowProjection, shadowClipPos);

	return shadowClipPos;
}

uniform float BiomeSandstorm, BiomeGreenShift, volFogDensity;
uniform vec3 volFogWind;
uniform float meWeight;

#if FOG_TYPE == 0
	/* Low */
	float CalculateFogDensity(in vec3 rayPosition) {
		float fogDensity = exp2(min((SEA_LEVEL + 32.0 - rayPosition.y) * rcp(12.0), 0.2));
		return fogDensity * 0.5;
	}
#elif FOG_TYPE == 1
	/* Medium */
	float CalculateFogDensity(in vec3 rayPosition) {
		float fogDensity = exp2(min((SEA_LEVEL + 28.0 - rayPosition.y) * 0.15, 0.2));

		rayPosition *= 0.07;
		rayPosition += volFogWind;
		float noise = Get3DNoiseSmooth(rayPosition) * 4.0;
		noise -= Get3DNoiseSmooth(rayPosition * 4.0 + volFogWind);

		fogDensity = saturate(noise * 4.0 * fogDensity - 5.0) * 1.4;
		if (BiomeSandstorm < 5e-3) fogDensity = fogDensity * oneMinus(timeNoon) + timeNoon;
		return fogDensity;
	}
#elif FOG_TYPE == 2
	/* High */
 	float CalculateFogDensity(in vec3 rayPosition) {
		float falloff = fastExp(-abs(rayPosition.y - SEA_LEVEL) * 0.01);

		rayPosition *= 0.04;
		rayPosition += volFogWind;
		float noise = Get3DNoiseSmooth(rayPosition) * 0.5;
			rayPosition += volFogWind;
		noise += Get3DNoiseSmooth(rayPosition * 3.2) * 0.25;
			rayPosition += volFogWind;
		noise += Get3DNoiseSmooth(rayPosition * 9.6) * 0.125;
			rayPosition += volFogWind;
		noise += Get3DNoiseSmooth(rayPosition * 28.8) * 0.0625;

		//noise = curve(smoothstep(0.2, 0.7, noise)) - falloff * 0.2;

		float fogDensity = saturate(noise * 12.0 * falloff - 4.5);
		return fogDensity * 9.0;
	}
#else
	/* Ultra */
	float CalculateFogDensity(in vec3 rayPosition) {
		float falloff = exp2(-abs(rayPosition.y - SEA_LEVEL) * 0.01);
		rayPosition += volFogWind;
		rayPosition *= 0.013;
		float weight = 0.5;
		float noise = 0.0;

		for (uint i = 0u; i < 5u; i++, weight *= 0.5) {
			noise += weight * Get3DNoiseSmooth(rayPosition);
			rayPosition = (rayPosition + volFogWind) * 4.0;
		}

		float fogDensity = saturate(falloff * noise * 4e2 - 1.7e2)/* * oneMinus(timeNoon) + timeNoon*/;
		return fogDensity * 48.0;
	}
#endif

const int shadowMapResolution = 2048;  // Shadowmap resolution [1024 2048 4096 8192 16384 32768]
const float realShadowMapRes = shadowMapResolution * MC_SHADOW_QUALITY;


vec4 CalculateVolumetricFog(in vec3 worldPos, in vec3 worldDir, in float dither) {	
	//worldPos *= min(1.0, far / length(worldPos));
	#if defined DISTANT_HORIZONS
		#define far float(dhRenderDistance)

		float rayLength = min(far + wetness * 3e-5 * dotSelf(worldPos.xz), length(worldPos));

		uint steps = VOLUMETRIC_FOG_SAMPLES;
	#else
		float rayLength = min(far + wetness * 3e-5 * dotSelf(worldPos.xz), length(worldPos));

		uint steps = uint(VOLUMETRIC_FOG_SAMPLES * 0.4 + rayLength * 0.1);
			 steps = min(steps, VOLUMETRIC_FOG_SAMPLES);
	#endif

	float rSteps = 1.0 / float(steps);

	//float dither = bayer64(gl_FragCoord.xy);
	//dither = fract(frameCounter / 7.0 + dither);
	//float dither = R1(frameCounter, texelFetch(noisetex, ivec2(gl_FragCoord.xy * 2) & 255, 0).a);

	float stepLength = rayLength * rSteps,
		  transmittance = 1.0,
		  LdotV = dot(worldLightVector, worldDir),
		  LdotV01 = LdotV * 0.5 + 0.5,
		  skylightSample = 0.0;

	float mistDensity = VOLUMETRIC_FOG_DENSITY * volFogDensity;
	#if FOG_TYPE > 1
	float phases1 = (HenyeyGreensteinPhase(LdotV, 0.6) 		   + HenyeyGreensteinPhase(LdotV, -0.3))		 * 0.5,
		  phases2 = (HenyeyGreensteinPhase(LdotV * 0.5, 0.6)   + HenyeyGreensteinPhase(LdotV * 0.5, -0.3))   * 0.25,
		  phases3 = (HenyeyGreensteinPhase(LdotV * 0.25, 0.6)  + HenyeyGreensteinPhase(LdotV * 0.25, -0.3))  * 0.125,
		  phases4 = (HenyeyGreensteinPhase(LdotV * 0.125, 0.6) + HenyeyGreensteinPhase(LdotV * 0.125, -0.3)) * 0.0625;
	#else
		mistDensity *= CornetteShanksPhase(LdotV, 0.7 - wetness * 0.3) * 0.45 + HenyeyGreensteinPhase(LdotV, -0.3) * 0.15 + 0.1;
	#endif
	#ifdef VOLUMETRIC_LIGHT
		float airDensity = VOLUMETRIC_LIGHT_STRENGTH + wetness * BiomeSandstorm;
		airDensity *= RayleighPhase(LdotV) * (3.0 / far);
	#else
		float airDensity = 0.0;
	#endif

	vec3 rayStep = worldDir * stepLength,
		 rayPosition = rayStep * dither + gbufferModelViewInverse[3].xyz + cameraPosition;

	vec3 shadowStart = WorldPosToShadowPos(gbufferModelViewInverse[3].xyz),
		 shadowEnd = WorldPosToShadowPos(rayStep + gbufferModelViewInverse[3].xyz);

	vec3 shadowStep = shadowEnd - shadowStart,
		 shadowPosition = shadowStep * dither + shadowStart;
	vec3 sunlightSample = vec3(0.0);

	#ifdef TIME_FADE
		airDensity *= max(saturate(meWeight + 0.25) + timeMidnight * 4.0, wetness);
		mistDensity *= max(sqr(meWeight) + timeMidnight * 2.0, wetness);
	#endif

	stepLength *= eyeSkylightFix;

	#ifdef CLOUDS_SHADOW
		CloudProperties cloudProperties = GetGlobalCloudProperties();
	#endif

	// for (uint i = 0u; i < steps; ++i, rayPosition += rayStep, shadowPosition += shadowStep) {
    uint i = 0u;
	while (++i < steps) {
		rayPosition += rayStep, shadowPosition += shadowStep;

        // if (rayPosition.y > 256.0) continue;
        if (rayPosition.y > 384.0) continue;
		vec3 shadowProjPos = DistortShadowSpace(shadowPosition) * 0.5 + 0.5;
		// if (saturate(shadowProjPos) != shadowProjPos) continue;
		ivec2 shadowTexel = ivec2(shadowProjPos.xy * realShadowMapRes);

		float fogDensity = airDensity;
		#ifdef VOLUMETRIC_FOG
			float density = CalculateFogDensity(rayPosition) * mistDensity;
			fogDensity += density;
		#endif

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

		#if defined VOLUMETRIC_FOG && FOG_TYPE > 1
			if (density > 1e-5) {
				float stepSize = 5.0, sunlightOD = 0.0;
				vec3 checkPos = rayPosition;
				for (uint i = 0u; i < 4u; ++i, checkPos += worldLightVector * stepSize) {
					float density = CalculateFogDensity(checkPos);
					if (density < 1e-5) continue;
					sunlightOD += density * stepSize;
					stepSize *= 1.5;
				}
				sunlightOD *= mistDensity;
				// Powder Effect
				float scatteringSun = oneMinus(fastExp(-sunlightOD * 2.0)) * oneMinus(LdotV01) + LdotV01;
				scatteringSun *= fastExp(-sunlightOD * 2.4) * phases1
							+ fastExp(-sunlightOD * 1.2) * phases2
							+ fastExp(-sunlightOD * 0.6) * phases3
							+ fastExp(-sunlightOD * 0.3) * phases4;
				shadow *= (scatteringSun + airDensity) * FOG_TYPE * FOG_TYPE;
			}

			float stepTransmittance = fastExp(-fogDensity);
		#else
			float stepTransmittance = fastExp(-fogDensity * (1.0 + BiomeSandstorm * wetness));
		#endif

 		// Powder Effect
		float powder = 1.0 - fastExp(-fogDensity * 3.0);
		powder = powder * oneMinus(LdotV01) + LdotV01;
		float fogSample = powder * transmittance * oneMinus(stepTransmittance);

		#ifdef CLOUDS_SHADOW
			float cloudShadow = CalculateCloudShadow(rayPosition, cloudProperties);
			shadow *= cloudShadow;
		#endif

		sunlightSample += shadow * fogSample;
		//sunlightSample += shadow * fogSample;
		skylightSample += fogSample;

		transmittance *= stepTransmittance;

		if (transmittance < 1e-3) break;
	}

	vec3 fogSunColor = directIlluminance * sunlightSample * SUNLIGHT_INTENSITY;
	vec3 fogSkyColor = skyIlluminance * skylightSample;

	vec3 fogColor = fogSunColor * 20.0 + fogSkyColor * 2.0;

	if (isLightningFlashing > 1e-2) fogColor += sqr(skylightSample) * 2.0 * lightningColor;

	if (BiomeSandstorm + BiomeGreenShift > 5e-3) {
		fogColor *= oneMinus(BiomeSandstorm) + vec3(0.42, 0.39, 0.21) * BiomeSandstorm;
		fogColor *= oneMinus(BiomeGreenShift) + vec3(0.7, 1.0, 0.74) * BiomeGreenShift;
	}

	fogColor *= oneMinus(0.8 * wetness);

	return vec4(fogColor, transmittance);
}

//------------------------------------------------------------------------------------------------//

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

	vec3 fogColor = 8.0 / coeff * directIlluminance * oneMinus(0.95 * wetness)// * oneMinus(stepTransmittance)
	;
	fogColor *= scattering * phase * UW_VOLUMETRIC_LIGHT_STRENGTH;

	return fogColor * SUNLIGHT_INTENSITY;
}