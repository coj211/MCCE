
#define CLOUD_CUMULUS_CLEAR_ALTITUDE	1000 // [60 80 100 150 200 300 356 400 500 600 700 800 1000 1200 1500 2000 5000 10000]

#define CLOUD_CUMULUS_CLEAR_THICKNESS	1400 // [0 100 200 300 400 500 550 600 700 800 1000 1200 1400 1500 1800 2000 3000 5000 6000]

#define CLOUD_CUMULUS_CLEAR_COVERY		1.0  // [0.5 0.7 0.8 0.9 1.0 1.1 1.15 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.3 2.5 2.7 3.0]

#define CLOUD_CUMULUS_CLEAR_DENSITY 	1.0  // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.5 3.0 5.0]

#define CLOUD_CUMULUS_CLEAR_SUNLIGHTING	1.0  // [0.1 0.3 0.35 0.4 0.45 0.5 0.7 0.9 1.0 1.1 1.3 1.5 1.7 1.9 2.1 2.3 2.5]

#define CLOUD_CUMULUS_CLEAR_SKYLIGHTING	1.0	 // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.5 3.0 5.0]


#define CLOUD_CUMULUS_RAIN_ALTITUDE		800  // [60 80 100 150 200 300 356 400 500 600 700 800 1000 1200 1500 2000 5000 10000]

#define CLOUD_CUMULUS_RAIN_THICKNESS	3000 // [0 100 200 300 400 465 500 600 800 1000 1200 1500 2000 2500 3000 3500 5000 6000 7000]

#define CLOUD_CUMULUS_RAIN_COVERY		1.2  // [0.5 0.7 0.8 0.9 1.0 1.1 1.15 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.3 2.5 2.7 3.0]

#define CLOUD_CUMULUS_RAIN_DENSITY 		1.0  // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.5 3.0 5.0]

#define CLOUD_CUMULUS_RAIN_SUNLIGHTING	0.3  // [0.1 0.15 0.2 0.3 0.5 0.7 0.9 1.1 1.3 1.5 1.7 1.9 2.1 2.3 2.5]

#define CLOUD_CUMULUS_RAIN_SKYLIGHTING	0.3	 // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.5 3.0 5.0]


#define CLOUD_CUMULUS_SAMPLES 			32 	 // [4 6 8 10 12 14 16 18 20 22 24 26 28 30 32 36 40 50 60 100]

#define CLOUD_CUMULUS_SUNLIGHT_SAMPLES 	4 	 // [2 3 4 5 6 7 8 9 10 12 15 17 20]
#define CLOUD_CUMULUS_SKYLIGHT_SAMPLES 	2 	 // [2 3 4 5 6 7 8 9 10 12 15 17 20]

#define CLOUD_LOCAL_COVERAGE


//------------------------------------------------------------------------------------------------//

struct CloudProperties {
	float altitude;
	float thickness;
	float coverage;
	float density;
	float sunlighting, skylighting;
	float maxAltitude;
	float noiseScale;
	float cloudPeakWeight;
};

#ifdef CLOUDS_WEATHER
	flat in vec3 cloudDynamicWeather;
#endif

CloudProperties GetGlobalCloudProperties() {
	CloudProperties cloudProperties;

	#ifdef CLOUDS_WEATHER
		if (cloudDynamicWeather.z > 5e-3) {
			cloudProperties.altitude    = mix(CLOUD_CUMULUS_CLEAR_ALTITUDE,	   CLOUD_CUMULUS_RAIN_ALTITUDE,	   wetness) * (1.0 + cloudDynamicWeather.z * 2.0);
			cloudProperties.density     = mix(CLOUD_CUMULUS_CLEAR_DENSITY,	   CLOUD_CUMULUS_RAIN_DENSITY,	   wetness) * oneMinus(cloudDynamicWeather.z * 0.3);
			cloudProperties.sunlighting = mix(CLOUD_CUMULUS_CLEAR_SUNLIGHTING, CLOUD_CUMULUS_RAIN_SUNLIGHTING, wetness) * (1.0 + cloudDynamicWeather.z * 0.2);
			cloudProperties.skylighting = mix(CLOUD_CUMULUS_CLEAR_SKYLIGHTING, CLOUD_CUMULUS_RAIN_SKYLIGHTING, wetness) * (1.0 + cloudDynamicWeather.z * 0.2);
		} else
	#endif
	{
		cloudProperties.altitude    = mix(CLOUD_CUMULUS_CLEAR_ALTITUDE,	   CLOUD_CUMULUS_RAIN_ALTITUDE,	   wetness);
		cloudProperties.density     = mix(CLOUD_CUMULUS_CLEAR_DENSITY,	   CLOUD_CUMULUS_RAIN_DENSITY,	   wetness);
		cloudProperties.sunlighting = mix(CLOUD_CUMULUS_CLEAR_SUNLIGHTING, CLOUD_CUMULUS_RAIN_SUNLIGHTING, wetness);
		cloudProperties.skylighting = mix(CLOUD_CUMULUS_CLEAR_SKYLIGHTING, CLOUD_CUMULUS_RAIN_SKYLIGHTING, wetness);
	}
	cloudProperties.thickness   = mix(CLOUD_CUMULUS_CLEAR_THICKNESS,   CLOUD_CUMULUS_RAIN_THICKNESS,   wetness);
	cloudProperties.coverage    = mix(CLOUD_CUMULUS_CLEAR_COVERY,	   CLOUD_CUMULUS_RAIN_COVERY,	   wetness);
	cloudProperties.maxAltitude	= cloudProperties.altitude + cloudProperties.thickness;
	cloudProperties.noiseScale 	= 4e-4 + 6e-5 * wetness;
	cloudProperties.cloudPeakWeight = 0.1 + 0.7 * wetness;

	return cloudProperties;
}

//------------------------------------------------------------------------------------------------//


// uniform sampler3D colortex8;

vec3 wind = vec3(2e-3, 2e-4, 1e-3) * worldTimeCounter * CLOUDS_SPEED;

float cloudForwardG = 0.6 - wetness * 0.2, cloudBackwardG = -0.4 + wetness * 0.2;
const float cloudBackwardWeight = 0.25, octWeight = 0.5, octScale = 3.0;

float CloudVolumeDensity(in CloudProperties cloudProperties, in vec3 worldPos, in uint steps, in float noiseDetail) {
	#ifdef CLOUD_LOCAL_COVERAGE
		float localCoverage = texture(noisetex, worldPos.xz * 2e-7 - wind.xz * 2e-3).y;
		localCoverage = saturate(fma(localCoverage, 3.0, wetness - 0.4)) * 0.5 + 0.5;
		if (localCoverage < 0.1) return 0.0;
    #endif

	vec3 position = worldPos * cloudProperties.noiseScale - wind;

	float density = noiseDetail * 0.03, weight = 0.5;

    for (uint i = 0u; i < steps; ++i, weight *= octWeight) {
		density += weight * Get3DNoiseSmooth(position);
        position = position * octScale - wind;
    }

	density += octWeight / octScale / steps;

	// vec4 lowFreqNoises = texture(colortex8, fract(position));
	// float fbm = lowFreqNoises.g * 0.625 + lowFreqNoises.b * 0.25 + lowFreqNoises.a * 0.125;

	// float density = saturate(remap(fbm - 1.0, 1.0, lowFreqNoises.x));

	if (density < 1e-6) return 0.0;

    #ifdef CLOUD_LOCAL_COVERAGE
		density *= localCoverage;
    #endif

	float normalizedHeight  = saturate((worldPos.y - cloudProperties.altitude) * rcp(cloudProperties.thickness));
	float heightAttenuation = saturate(normalizedHeight * 6.6) * saturate(oneMinus(normalizedHeight) * (2.0 + wetness));

	density = cloudProperties.coverage == 1.0 ? density : saturate((density - 1.0 + cloudProperties.coverage) * rcp(cloudProperties.coverage));

	density *= heightAttenuation * 1.9;
	density -= heightAttenuation * 0.9 + normalizedHeight * 0.5 + 0.1;

	return saturate(density * 3.0 * cloudProperties.density);
}

#ifdef CLOUDS_SHADOW
	float GetShadow3DNoiseSmooth(in vec3 position) {
		vec3 p = floor(position);
		vec3 b = curve(position - p);

		ivec2 texel = ivec2(p.xy + 97.0 * p.z);

		vec2 s0 = texelFetch(noisetex, texel % noiseTextureResolution, 0).xy;
		vec2 s1 = texelFetch(noisetex, (texel + ivec2(1, 0)) % noiseTextureResolution, 0).xy;
		vec2 s2 = texelFetch(noisetex, (texel + ivec2(0, 1)) % noiseTextureResolution, 0).xy;
		vec2 s3 = texelFetch(noisetex, (texel + ivec2(1, 1)) % noiseTextureResolution, 0).xy;

		vec2 rg = mix(mix(s0, s1, b.x), mix(s2, s3, b.x), b.y);

		return mix(rg.x, rg.y, b.z);
	}

	float CloudVolumeDensitySmooth(in CloudProperties cloudProperties, in vec3 worldPos) {
		#ifdef CLOUD_LOCAL_COVERAGE
			float localCoverage = texture(noisetex, worldPos.xz * 2e-7 - wind.xz * 2e-3 + 0.5).y;
			localCoverage = saturate(fma(localCoverage, 3.0, wetness - 0.4)) * 0.5 + 0.5;
			if (localCoverage < 0.1) return 0.0;
		#endif

		vec3 position = worldPos * cloudProperties.noiseScale - wind;

		float density = 0.03, weight = 0.5;

		for (uint i = 0u; i < 4u; ++i, weight *= octWeight) {
			density += weight * GetShadow3DNoiseSmooth(position);
			position = (position - wind) * octScale;
		}

		density += octWeight / octScale * 0.25;

		if (density < 1e-6) return 0.0;

		#ifdef CLOUD_LOCAL_COVERAGE
			density *= localCoverage;
		#endif

		float normalizedHeight  = saturate((worldPos.y - cloudProperties.altitude) * rcp(cloudProperties.thickness));
		float heightAttenuation = saturate(normalizedHeight * 6.6) * saturate(oneMinus(normalizedHeight) * (2.0 + wetness));

		density = cloudProperties.coverage == 1.0 ? density : saturate((density - 1.0 + cloudProperties.coverage) * rcp(cloudProperties.coverage));

		density *= heightAttenuation * 1.9;
		density -= heightAttenuation * 0.9 + normalizedHeight * 0.5 + 0.1;

		return saturate(density * 3.0 * cloudProperties.density);
	}
#endif

float GetNoiseDetail(in vec3 worldDir) {
	//worldDir = worldDir * 5.0 - wind;
	worldDir *= 48.0;

	//float pnoise = 	texture3D(colortex4, fract(worldDir)).z; 		 		worldDir += pnoise * 1e-2 - wind;
	//pnoise +=  		texture3D(colortex4, fract(worldDir * 2.0)).z * 0.5;	worldDir += pnoise * 1e-2 - wind;
	//pnoise +=  		texture3D(colortex4, fract(worldDir * 4.0)).z * 0.25;	worldDir += pnoise * 1e-3 - wind;
	//pnoise +=  		texture3D(colortex4, fract(worldDir * 8.0)).z * 0.125;
	float pnoise = 	Get3DNoise(worldDir - wind); 		 worldDir += pnoise * 1e-3 - wind;
	pnoise +=  		Get3DNoise(worldDir * 2.0);			 worldDir += pnoise * 1e-3 - wind;
	pnoise +=  		Get3DNoise(worldDir * 4.0) * 0.5;	 worldDir += pnoise * 1e-3 - wind;
	pnoise +=  		Get3DNoise(worldDir * 8.0) * 0.25;	 worldDir += pnoise * 1e-3 - wind;
	pnoise +=  		Get3DNoise(worldDir * 16.0) * 0.125; worldDir += pnoise * 1e-3 - wind;

	//return pnoise * 1.2 - 0.1;
	return pnoise - 0.15;
}

//vec3 cumulusSunlightColor = CumulusSunlightColor();

float CloudVolumeSunLightOD(in CloudProperties cloudProperties, in vec3 rayPos, in float lightNoise) {
    float rayLength = cloudProperties.thickness * (0.2 / float(CLOUD_CUMULUS_SUNLIGHT_SAMPLES));
	vec4 rayStep = vec4(worldLightVector, 1.0) * rayLength;

    float opticalDepth = 0.0;

	for (uint i = 0u; i < CLOUD_CUMULUS_SUNLIGHT_SAMPLES; ++i, rayPos += rayStep.xyz) {
        rayStep *= 2.0;
		// if (rayPos.y < cloudProperties.altitude || rayPos.y > cloudProperties.maxAltitude) continue;

		float density = CloudVolumeDensity(cloudProperties, rayPos + rayStep.xyz * lightNoise, 5u, 1.0);
		if (density < 1e-4) continue;

        // opticalDepth += density * rayStep.w;
        opticalDepth += density;
    }

    return opticalDepth * rayLength * 0.12;
}

float CloudVolumeSkyLightOD(in CloudProperties cloudProperties, in vec3 rayPos, in float lightNoise) {
    float rayLength = cloudProperties.thickness * (0.2 / float(CLOUD_CUMULUS_SKYLIGHT_SAMPLES));
	vec4 rayStep = vec4(vec3(0.0, 1.0, 0.0), 1.0) * rayLength;

    float opticalDepth = 0.0;

	for (uint i = 0u; i < CLOUD_CUMULUS_SKYLIGHT_SAMPLES; ++i, rayPos += rayStep.xyz) {
        rayStep *= 2.0;
		// if (rayPos.y < cloudProperties.altitude || rayPos.y > cloudProperties.maxAltitude) continue;

		float density = CloudVolumeDensity(cloudProperties, rayPos + rayStep.xyz * lightNoise, 3u, 1.0);
		if (density < 1e-4) continue;

        // opticalDepth += density * rayStep.w;
        opticalDepth += density;
    }

    // return opticalDepth * 0.04;
    return opticalDepth * rayLength * 0.04;
}

// vec4 VolumetricClouds(in vec3 worldDir, in float dither, in float lightNoise, in vec4 phases, in CloudProperties cloudProperties, inout float transmittance) {
// 	//CloudProperties cp = cloudProperties;

// 	if ((worldDir.y < 0.0 && eyeAltitude < cloudProperties.altitude)
// 	 || (worldDir.y > 0.0 && eyeAltitude > cloudProperties.maxAltitude)) return vec4(0.0, 0.0, 0.0, 1.0);

// 	vec3 planeOrigin = vec3(0.0, planetRadius + eyeAltitude, 0.0);
// 	vec2 bottomIntersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + cloudProperties.altitude);
// 	vec2 topIntersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + cloudProperties.maxAltitude);

// 	float startLength, endLength;
// 	if (eyeAltitude > cloudProperties.maxAltitude) {
// 		startLength = topIntersection.x;
// 		endLength = bottomIntersection.x;
// 	} else {
// 		startLength = bottomIntersection.y;
// 		endLength = topIntersection.y;
// 	}

// 	// The range of eye in cloudsData
// 	float rayRange = oneMinus(saturate((eyeAltitude - cloudProperties.maxAltitude) * 0.1)) *
// 					 oneMinus(saturate((cloudProperties.altitude - eyeAltitude) * 0.1));

// 	// The ray distance in range
// 	float rayDist = bottomIntersection.y >= 0.0 && eyeAltitude > cloudProperties.altitude ? bottomIntersection.x : topIntersection.y;
// 	//rayDist = min(rayDist, cloudProperties.altitude * 10.0);

// 	startLength *= oneMinus(rayRange);
// 	endLength = mix(endLength, rayDist, rayRange);

// 	uint raySteps = CLOUD_CUMULUS_SAMPLES;
// 	raySteps = uint(mix(raySteps, uint(raySteps / 1.6), abs(worldDir.y))); // Steps Fade

//     float rayLength = clamp(endLength - startLength, 0.0, 2e4) * rcp(raySteps);
// 	vec3 rayStep = rayLength * worldDir;
// 	vec3 rayPos = (startLength + rayLength * dither) * worldDir + cameraPosition;

// 	//float dist = distance(rayPos, cameraPosition);
// 	//float noiseDetail = mix(GetNoiseDetail(worldDir), 1.0, exp2(-dist * 0.0007f) * 0.8) * 0.03;
// 	float noiseDetail = GetNoiseDetail(worldDir);
// 	//float lightNoise = InterleavedGradientNoise();

// 	//uint raySteps = max(uint(CLOUD_CUMULUS_SAMPLES - sqrt(dist) * 0.06), CLOUD_CUMULUS_SAMPLES / 2);
// 	//uint raySteps = uint(CLOUD_CUMULUS_SAMPLES);

// 	//float LdotV01 = dot(worldDir, worldLightVector) * 0.5 + 0.5;
// 	//float LdotV = dot(worldDir, worldLightVector);

// 	float scatteringSun = 0.0;
// 	float scatteringSky = 0.0;

//     //float powderIntensity = saturate(CornetteShanksPhase(LdotV, 0.5));
//     //float powderIntensity = 0.8 * sqr(dot(worldDir, worldLightVector) * 0.5 + 0.5);

// 	for (uint i = 0u; i < raySteps; ++i, rayPos += rayStep) {
// 		if (transmittance < minTransmittance) break;
//         if (rayPos.y < cloudProperties.altitude || rayPos.y > cloudProperties.maxAltitude) continue;

// 		float dist = distance(rayPos, cameraPosition);
// 		if (dist > 1e5 - 3e4 * wetness) continue;

// 		float density = CloudVolumeDensity(cloudProperties, rayPos, 5u, mix(noiseDetail, 1.0, exp2(-dist * 0.001f)));
// 		if (density < 1e-4) continue;

// 		float sunlightOD = CloudVolumeSunLightOD(cloudProperties, rayPos, lightNoise);

// 		//float powder = 1.0 - fastExp(-density * 32.0);
// 	    //powder = powder * oneMinus(powderIntensity) + powderIntensity;
// 		float powder = oneMinus(fastExp(-density * 32.0)) * 0.82;
// 	    powder /= 1.0 - powder;

// 		//float sunlightEnergy = 	fastExp(-sunlightOD * 2.0) * phases.x;
// 		//sunlightEnergy += 		fastExp(-sunlightOD * 0.8) * phases.y;
// 		//sunlightEnergy += 		fastExp(-sunlightOD * 0.3) * phases.z;
// 		//sunlightEnergy += 		fastExp(-sunlightOD * 0.1) * phases.w;
// 		float sunlightEnergy = 	rcp(sunlightOD * 2.0 + 1.0) * phases.x;
// 		sunlightEnergy += 		rcp(sunlightOD * 0.9 + 1.0) * phases.y;
// 		sunlightEnergy += 		rcp(sunlightOD * 0.4 + 1.0) * phases.z;
// 		sunlightEnergy += 		rcp(sunlightOD * 0.2 + 1.0) * phases.w;

// 		float skylightEnergy = CloudVolumeSkyLightOD(cloudProperties, rayPos, lightNoise);
// 		skylightEnergy = fastExp(-skylightEnergy) + fastExp(-skylightEnergy * 0.1) * 0.1;

// 		float stepTransmittance = fastExp(-density * 0.1 * rayLength);
// 		float cloudSample = powder * transmittance * oneMinus(stepTransmittance);
// 		scatteringSun += sunlightEnergy * cloudSample;
// 		scatteringSky += skylightEnergy * cloudSample;
// 		transmittance *= stepTransmittance;	
// 	}
// 	if (transmittance > 0.999) return vec4(0.0, 0.0, 0.0, 1.0);

// 	vec3 scattering = scatteringSun * 64.0 * cloudProperties.sunlighting * SunAbsorptionAtAltitude(1.0);
// 	scattering *= saturate(worldLightVector.y * 40.0);
// 	if (cloudMoonlit) scattering = DoNightEye(scattering * MoonFlux);
// 	scattering += scatteringSky * 0.2 * cloudProperties.skylighting * skyIlluminance;

// 	// lightning
// 	if (isLightningFlashing > 1e-2) scattering += sqr(scatteringSky) * 0.1 * lightningColor;

// 	#ifdef AURORA
// 		scattering += sqr(scatteringSky) * vec3(0.0, 0.002, 0.001) * auroraAmount;
// 	#endif

// 	float atmosFade = fastExp(-distance(rayPos, cameraPosition) * (0.03 + 0.02 * wetness) * rcp(cloudProperties.altitude));
// 	//color *= 1.0 - atmosFade + transmittance * atmosFade;

// 	vec3 atmos = Atmosphere(worldDir, worldSunVector, 1.0, endLength * 1e-4);
// 	//atmos += DoNightEye(Atmosphere(worldDir, -worldSunVector, 1.0, endLength * 1e-4) * MoonFlux);
// 	//color += (scattering + atmos) * atmosFade;
// 	//color += scattering * atmosFade;

// 	return vec4((scattering + atmos) * atmosFade, 1.0 - atmosFade + transmittance * atmosFade);
// }

// vec4 VolumetricCloudsRef(in vec3 worldDir, in vec4 phases, in CloudProperties cloudProperties, inout float transmittance) {
// 	//CloudProperties cp = cloudProperties;

// 	if ((worldDir.y < 0.0 && eyeAltitude < cloudProperties.altitude)
// 	 || (worldDir.y > 0.0 && eyeAltitude > cloudProperties.maxAltitude)) return vec4(0.0, 0.0, 0.0, 1.0);

// 	vec3 planeOrigin = vec3(0.0, planetRadius + eyeAltitude, 0.0);
// 	vec2 bottomIntersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + cloudProperties.altitude);
// 	vec2 topIntersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + cloudProperties.maxAltitude);

// 	float startLength, endLength;
// 	if (eyeAltitude > cloudProperties.maxAltitude) {
// 		startLength = topIntersection.x;
// 		endLength = bottomIntersection.x;
// 	} else {
// 		startLength = bottomIntersection.y;
// 		endLength = topIntersection.y;
// 	}

// 	// The range of eye in cloudsData
// 	float rayRange = oneMinus(saturate((eyeAltitude - cloudProperties.maxAltitude) * 0.1)) *
// 					 oneMinus(saturate((cloudProperties.altitude - eyeAltitude) * 0.1));

// 	// The ray distance in range
// 	float rayDist = bottomIntersection.y >= 0.0 && eyeAltitude > cloudProperties.altitude ? bottomIntersection.x : topIntersection.y;
// 	//rayDist = min(rayDist, cloudProperties.altitude * 10.0);

// 	startLength *= oneMinus(rayRange);
// 	endLength = mix(endLength, rayDist, rayRange);

// 	uint raySteps = uint(CLOUD_CUMULUS_SAMPLES * 0.6);
// 	raySteps = uint(mix(raySteps, uint(raySteps / 1.6), abs(worldDir.y))); // Steps Fade

//     float rayLength = clamp(endLength - startLength, 0.0, 2e4) * rcp(raySteps);
// 	vec3 rayStep = rayLength * worldDir;
// 	vec3 rayPos = (startLength + rayLength * 0.5) * worldDir + cameraPosition;

// 	//float dist = distance(rayPos, cameraPosition);
// 	//float noiseDetail = mix(GetNoiseDetail(worldDir), 1.0, exp2(-dist * 0.0007f) * 0.8) * 0.03;
// 	float noiseDetail = GetNoiseDetail(worldDir);
// 	//float lightNoise = InterleavedGradientNoise();

// 	//uint raySteps = max(uint(CLOUD_CUMULUS_SAMPLES - sqrt(dist) * 0.06), CLOUD_CUMULUS_SAMPLES / 2);
// 	//uint raySteps = uint(CLOUD_CUMULUS_SAMPLES);

// 	//float LdotV01 = dot(worldDir, worldLightVector) * 0.5 + 0.5;
// 	//float LdotV = dot(worldDir, worldLightVector);

// 	float scatteringSun = 0.0;
// 	float scatteringSky = 0.0;

//     //float powderIntensity = saturate(CornetteShanksPhase(LdotV, 0.5));
//     //float powderIntensity = 0.8 * sqr(dot(worldDir, worldLightVector) * 0.5 + 0.5);

// 	for (uint i = 0u; i < raySteps; ++i, rayPos += rayStep) {
// 		if (transmittance < minTransmittance) break;
//         if (rayPos.y < cloudProperties.altitude || rayPos.y > cloudProperties.maxAltitude) continue;

// 		float dist = distance(rayPos, cameraPosition);
// 		if (dist > 1e5 - 3e4 * wetness) continue;

// 		float density = CloudVolumeDensity(cloudProperties, rayPos, 5u, mix(noiseDetail, 1.0, exp2(-dist * 0.001f)));
// 		if (density < 1e-4) continue;

// 		float sunlightOD = CloudVolumeSunLightOD(cloudProperties, rayPos, 0.5);

// 		//float powder = 1.0 - fastExp(-density * 32.0);
// 	    //powder = powder * oneMinus(powderIntensity) + powderIntensity;
// 		float powder = oneMinus(fastExp(-density * 32.0)) * 0.82;
// 	    powder /= 1.0 - powder;

// 		//float sunlightEnergy = 	fastExp(-sunlightOD * 2.0) * phases.x;
// 		//sunlightEnergy += 		fastExp(-sunlightOD * 0.8) * phases.y;
// 		//sunlightEnergy += 		fastExp(-sunlightOD * 0.3) * phases.z;
// 		//sunlightEnergy += 		fastExp(-sunlightOD * 0.1) * phases.w;
// 		float sunlightEnergy = 	rcp(sunlightOD * 2.0 + 1.0) * phases.x;
// 		sunlightEnergy += 		rcp(sunlightOD * 0.9 + 1.0) * phases.y;
// 		sunlightEnergy += 		rcp(sunlightOD * 0.4 + 1.0) * phases.z;
// 		sunlightEnergy += 		rcp(sunlightOD * 0.2 + 1.0) * phases.w;

// 		float skylightEnergy = CloudVolumeSkyLightOD(cloudProperties, rayPos, 0.5);
// 		skylightEnergy = fastExp(-skylightEnergy) + fastExp(-skylightEnergy * 0.1) * 0.1;

// 		float stepTransmittance = fastExp(-density * 0.1 * rayLength);
// 		float cloudSample = powder * transmittance * oneMinus(stepTransmittance);
// 		scatteringSun += sunlightEnergy * cloudSample;
// 		scatteringSky += skylightEnergy * cloudSample;
// 		transmittance *= stepTransmittance;	
// 	}
// 	if (transmittance > 0.999) return vec4(0.0, 0.0, 0.0, 1.0);

// 	vec3 scattering = scatteringSun * 64.0 * cloudProperties.sunlighting * SunAbsorptionAtAltitude(1.0);
// 	scattering *= saturate(worldLightVector.y * 40.0);
// 	if (cloudMoonlit) scattering = DoNightEye(scattering * MoonFlux);
// 	scattering += scatteringSky * 0.2 * cloudProperties.skylighting * skyIlluminance;

// 	// lightning
// 	if (isLightningFlashing > 1e-2) scattering += sqr(scatteringSky) * 0.1 * lightningColor;

// 	#ifdef AURORA
// 		scattering += sqr(scatteringSky) * vec3(0.0, 0.002, 0.001) * auroraAmount;
// 	#endif

// 	float atmosFade = fastExp(-distance(rayPos, cameraPosition) * (0.03 + 0.02 * wetness) * rcp(cloudProperties.altitude));
// 	//color *= 1.0 - atmosFade + transmittance * atmosFade;

// 	vec3 atmos = Atmosphere(worldDir, worldSunVector, 1.0, endLength * 1e-4);
// 	//atmos += DoNightEye(Atmosphere(worldDir, -worldSunVector, 1.0, endLength * 1e-4) * MoonFlux);
// 	//color += (scattering + atmos) * atmosFade;
// 	//color += scattering * atmosFade;

// 	return vec4((scattering + atmos) * atmosFade, 1.0 - atmosFade + transmittance * atmosFade);
// }
