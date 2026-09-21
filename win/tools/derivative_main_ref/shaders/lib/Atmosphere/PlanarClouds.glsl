
#define CLOUD_PLANE_ALTITUDE 7000 // [400 500 1000 1200 1500 1700 2000 3000 4000 5000 6000 6500 7000 7500 8000 9000 10000 12000]

#define CLOUD_PLANE0_DENSITY 1.0 // [0 0.1 0.2 0.3 0.4 0.6 0.8 1.0 1.2 1.5 1.7 2.0 3.0 5.0 7.5 10.0]
#define CLOUD_PLANE0_COVERY 0.5 // [0 0.05 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.65 0.7 0.8 0.9 1.0]

#define CLOUD_PLANE1_DENSITY 1.0 // [0 0.1 0.2 0.3 0.4 0.6 0.8 1.0 1.2 1.5 1.7 2.0 3.0 5.0 7.5 10.0]
#define CLOUD_PLANE1_COVERY 0.5 // [0 0.05 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.65 0.7 0.8 0.9 1.0]


//------------------------------------------------------------------------------------------------//

#if CIRRUS_CLOUDS == 1
	float GetCloudsNoise(vec2 position) { return texture(noisetex, position * 1e-2).a; }

	vec4 PlanarSample0(in float dist, in vec2 worldPos, in float LdotV) {
		//wind.xz *= 50.0;
    	worldPos /= 1.0 + distance(worldPos, cameraPosition.xz) * 5e-6;
		vec2 position = worldPos * 4e-5 - wind.xz;
		position += texture(noisetex, position * 0.04).y * 0.1;

    	float localCoverage = texture(noisetex, position * 2e-3 + 0.15).x;

		const float goldenAngle = TAU / (PHI1 + 1.0);
		const mat2 goldenRotate = mat2(cos(goldenAngle), -sin(goldenAngle), sin(goldenAngle), cos(goldenAngle));

		float amplitude = 0.5;
		float noise = GetCloudsNoise(position);
		for (uint i = 1u; i < 6u; ++i, amplitude *= 0.43) {
			position = goldenRotate * 3.2 * (position - wind.xz);
			noise += GetCloudsNoise(position * (1.0 + vec2(-0.35, 0.05) * sqrt(i))) * amplitude;
		}

        noise -= saturate(localCoverage * 4.0 - 1.6);
		#ifdef CLOUDS_WEATHER
			noise -= cloudDynamicWeather.y;
		#endif
		noise = saturate(noise * 1.36 + CLOUD_PLANE0_COVERY - 1.7) * noise;

		//float localDensity = texture(noisetex, wind.xz * 2e-2 + worldPos * 4e-7).x;
		//noise *= sqr(localDensity * 2.1 * PC_COVERY - 0.4);
		if (noise < 1e-5) return vec4(0.0);
		//noise = max0(noise * sqrt(noise));

		float powder = oneMinus(fastExp(-noise * 2.4)) * 0.7;
	    powder /= 1.0 - powder;

		float phase = MiePhaseClouds(LdotV, vec3(-0.2, 0.5, 0.9), vec3(0.3, 0.6, 0.1))// + 0.25 * rPI
		;

		bool moonlit = worldSunVector.y < -0.049;

		vec3 lightColor = phase * (moonlit ? moonIlluminance : sunIlluminance) * 17.0;
		lightColor += skyIlluminance * 0.1;

		// lightning
		if (isLightningFlashing > 1e-2) lightColor += lightningColor * 2.5;

		#ifdef AURORA
			lightColor += vec3(0.0, 0.05, 0.025) * auroraAmount;
		#endif

		lightColor *= oneMinus(0.8 * wetness);
			//color *= abs(worldLightVector.y) + 2.0;
			//color *= 1.0 + LdotV01 * LdotV01;
		noise = 1.0 - fastExp(-noise * 1.6 * CLOUD_PLANE0_DENSITY);
		//noise = 1.0 - fastExp(-noise * 5e-4 * dist * CLOUD_PLANE0_DENSITY);

		return vec4(lightColor * powder * noise, noise);
	}
#elif CIRRUS_CLOUDS == 2
	vec4 PlanarSample0(in float dist, in vec2 worldPos, in float LdotV) {
		//wind.xz *= 50.0;
    	worldPos /= 1.0 + distance(worldPos, cameraPosition.xz) * 5e-6;
		vec2 position = worldPos * 8e-7;
		//position += texture(noisetex, position * 2e-4).z;

    	float localCoverage = texture(noisetex, position * 0.1 - wind.xz * 2e-2).y;

		float weight = 0.5;
		float noise = texture(noisetex, position - wind.xz * 4e-2).x * weight;

		for (uint i = 1u; i < 6u; ++i) {
			weight *= 0.5;
			position *= vec2(2.0, 2.2 + sqrt(i));
			noise += texture(noisetex, position - curve(noise) * 0.3 * weight - wind.xz * 4e-2).x * weight;
		}

		//float localDensity = texture(noisetex, wind.xz * 2e-2 + worldPos * 4e-7).x;
		//noise *= sqr(localDensity * 2.1 * PC_COVERY - 0.4);
        noise -= saturate(localCoverage * 2.8 - 1.2);
		#ifdef CLOUDS_WEATHER
			noise -= cloudDynamicWeather.y;
		#endif
        noise = curve(saturate(noise * 2.0 + CLOUD_PLANE0_COVERY - 1.4) * noise);
		if (noise < 1e-5) return vec4(0.0);
		//noise = max0(noise * sqrt(noise));

		float powder = oneMinus(fastExp(-noise * 18.0)) * 0.7;
	    powder /= 1.0 - powder;

		float phase = MiePhaseClouds(LdotV, vec3(-0.2, 0.5, 0.9), vec3(0.3, 0.6, 0.1))// + 0.25 * rPI
		;

		bool moonlit = worldSunVector.y < -0.049;

		vec3 lightColor = phase * (moonlit ? moonIlluminance : sunIlluminance) * 25.0;
		lightColor += skyIlluminance * 0.1;

		// lightning
		if (isLightningFlashing > 1e-2) lightColor += lightningColor * 5.0;

		#ifdef AURORA
			lightColor += vec3(0.0, 0.05, 0.025) * auroraAmount;
		#endif

		lightColor *= oneMinus(0.8 * wetness);
			//color *= abs(worldLightVector.y) + 2.0;
			//color *= 1.0 + LdotV01 * LdotV01;
		noise = 1.0 - fastExp(-noise * 4.0 * CLOUD_PLANE0_DENSITY);
		//noise = 1.0 - fastExp(-noise * 5e-4 * dist * CLOUD_PLANE0_DENSITY);

		return vec4(lightColor * powder * noise, noise);
	}
#endif

#ifdef CIRROCUMULUS_CLOUDS
	//const float windAngle = PI / 60.0;
	//const mat2 rotateWindAngle = mat2(cos(windAngle), -sin(windAngle), sin(windAngle), cos(windAngle));

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

		return cube(saturate(noise * 4.0));
	}

	vec4 PlanarSample1(in float dist, in vec2 worldPos, in float LdotV, in float lightNoise, in vec4 phases, in vec3 worldDir) {
		//float LdotV01 = LdotV * 0.5 + 0.5;
		//float dist = distance(worldPos, cameraPosition.xz);
    	//float e0 = 0.5 / (0.5 * 0.2 / (1.0 * dist) + (1.0 - 0.5 * 0.2 / 1.0));

		float density = CloudPlanarDensity(worldPos);
		if (density < 1e-5) return vec4(0.0);

		float rayLength = 60.0;
		vec2 rayPos = worldPos;
		vec3 rayStep = vec3(worldLightVector.xz, 1.0) * rayLength;

		float opticalDepth = 0.0;

		for (uint i = 0u; i < 3u; ++i, rayPos += rayStep.xy) {
			rayStep *= 2.0;

			float density = CloudPlanarDensity(rayPos + rayStep.xy * lightNoise);
			if (density < 1e-4) continue;

			opticalDepth += density * rayStep.z;
			//opticalDepth += density;
		}
	
		//sunopticalDepth -= noiseDetail * 0.0021;
		//opticalDepth *= pow4(pow5(LdotV01)) + 1.6;
		opticalDepth *= /* oneMinus(abs(worldLightVector.y) * 0.4) *  */CLOUD_PLANE1_DENSITY;
		//float powder = 1.0 - fastExp(-density * 3e2);
		//float powder = TAU * density / (density * 2.0 + 0.15);
		float powder = oneMinus(fastExp(-density * 6e2)) * 0.75;
    	//float powderIntensity = 0.8 * sqr(LdotV * 0.5 + 0.5);
	    //powder = powder * oneMinus(powderIntensity) + powderIntensity;
		//float powder = TAU * density / (density * 2.0 + 0.15);
    	//float powderIntensity = 0.1 + 0.8 * sqr(LdotV * 0.5 + 0.5);
	    powder /= 1.0 - powder;

		float sunlightEnergy = 	fastExp(-opticalDepth * 1.0) * phases.x;
		sunlightEnergy += 		fastExp(-opticalDepth * 0.4) * phases.y;
		sunlightEnergy += 		fastExp(-opticalDepth * 0.15) * phases.z;
		sunlightEnergy += 		fastExp(-opticalDepth * 0.05) * phases.w;

    	//sunlightEnergy *= 1.2 + sqr(LdotV01);
		//sunlightEnergy *= 1.0 + MiePhaseCloud(LdotV, 0.7) * sunlightEnergy * fma(wetness, 0.1, 0.05);

		opticalDepth = 0.0;

		rayLength = 1e2;
		rayStep = vec3(worldDir.xz, 1.0) * rayLength;

		for (uint i = 0u; i < 2u; ++i, worldPos += rayStep.xy) {
			rayStep *= 2.0;

			float density = CloudPlanarDensity(worldPos + rayStep.xy * lightNoise);
			if (density < 1e-4) continue;

			opticalDepth += density * rayStep.z;
			//opticalDepth += density;
		}

		opticalDepth *= CLOUD_PLANE1_DENSITY;
		float skylightEnergy = fastExp(-opticalDepth * 0.15);
		skylightEnergy += 0.2 * fastExp(-opticalDepth * 0.03);
		vec3 scatteringSky = skylightEnergy * 0.3 * skyIlluminance;

		// lightning
		if (isLightningFlashing > 1e-2) scatteringSky += sqr(skylightEnergy) * 1.4 * lightningColor;

		#ifdef AURORA
			scatteringSky += skylightEnergy * vec3(0.0, 0.02, 0.01) * auroraAmount;
		#endif

		density = oneMinus(fastExp(-density * 2e-2 * CLOUD_PLANE1_DENSITY * dist));
		//density = oneMinus(fastExp(-density * sqrt(density) * 1.2e2));
		bool moonlit = worldSunVector.y < -0.045;

		vec3 scattering = sunlightEnergy * 1.2e2 * (moonlit ? moonIlluminance : sunIlluminance);
		scattering += scatteringSky;
		scattering *= oneMinus(0.7 * wetness);

		return vec4(scattering * powder * density, density);
	}
#endif

// vec4 PlanarClouds(in vec3 worldDir, in float dither, in vec4 phases, out float cloudTransmittance) {
// 	if ((worldDir.y < 0.0 && eyeAltitude < CLOUD_PLANE_ALTITUDE)
// 	 || (worldDir.y > 0.0 && eyeAltitude > CLOUD_PLANE_ALTITUDE)) return vec4(0.0, 0.0, 0.0, 1.0);

// 	vec3 planeOrigin = vec3(0.0, planetRadius + eyeAltitude, 0.0);
// 	vec2 intersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + CLOUD_PLANE_ALTITUDE);
//     float cloudDistance = eyeAltitude > CLOUD_PLANE_ALTITUDE ? intersection.x : intersection.y;

// 	if (cloudDistance <= 0.0) return vec4(0.0, 0.0, 0.0, 1.0);
// 	vec3 cloudPos = worldDir * cloudDistance;

// 	//float cloudDist = length(cloudPos);
// 	if (cloudDistance > 3e5 - 6e4 * wetness) return vec4(0.0, 0.0, 0.0, 1.0);

// 	float LdotV = dot(worldDir, worldLightVector);

// 	vec3 atmos = Atmosphere(worldDir, worldSunVector, 1.0, cloudDistance * 1.2e-4) * 0.5;
// 	//atmos += DoNightEye(Atmosphere(worldDir, -worldSunVector, 1.0, cloudDist * 2e-4) * MoonFlux);
// 	vec4 cloudSample = vec4(0.0, 0.0, 0.0, 1.0);
// 	cloudTransmittance = 1.0;

// 	#ifdef CLOUDS_WEATHER
// 		vec2 weatherMap = texelFetch(noisetex, ivec2(worldDay) % noiseTextureResolution, 0).yz;
// 	#endif

// 	#ifdef CIRROCUMULUS_CLOUDS
// 		#ifdef CLOUDS_WEATHER
// 			if (weatherMap.x > 0.43)
// 		#endif
// 		{
// 			float atmosFade = fastExp(-cloudDistance * fma(0.02, wetness, 0.12) * rcp(float(CLOUD_PLANE_ALTITUDE)));
// 			vec4 sampleTemp = PlanarSample1(cloudDistance, cameraPosition.xz + cloudPos.xz, LdotV, dither, phases, worldDir);
// 			sampleTemp.rgb += atmos * sampleTemp.a;
// 			cloudTransmittance *= 1.0 - sampleTemp.a;

// 			sampleTemp *= atmosFade;
// 			cloudSample.a *= 1.0 - sampleTemp.a;
// 			cloudSample.rgb += sampleTemp.rgb;
// 			//color.rgb = color.rgb * oneMinus(sampleTemp.a) + sampleTemp.rgb;
// 			//color.rgb = mix(color.rgb, sampleTemp.rgb, saturate(sampleTemp.a));
// 		}
// 	#endif
// 	#if CIRRUS_CLOUDS > 0
// 		#ifdef CLOUDS_WEATHER
// 			if (weatherMap.y < 0.45)
// 		#endif
// 		{
// 			float atmosFade = fastExp(-cloudDistance * fma(0.02, wetness, 0.12) * rcp(float(CLOUD_PLANE_ALTITUDE)));
// 			vec4 sampleTemp = PlanarSample0(cloudDistance, cameraPosition.xz + cloudPos.xz, LdotV);
// 			sampleTemp.rgb += atmos * sampleTemp.a;
// 			cloudTransmittance *= 1.0 - sampleTemp.a;

// 			sampleTemp *= atmosFade;
// 			cloudSample.a *= 1.0 - sampleTemp.a;
// 			cloudSample.rgb += sampleTemp.rgb;
// 			//color.rgb = color.rgb * oneMinus(sampleTemp.a) + sampleTemp.rgb;
// 			//color.rgb = mix(color.rgb, sampleTemp.rgb, saturate(sampleTemp.a));
// 			//cloudTransmittance *= 1.0 - cloudSample1.a;
// 		}
// 	#endif

// 	return cloudSample;
// }

// vec4 PlanarCloudsRef(in vec3 worldDir, in vec4 phases, out float cloudTransmittance) {
// 	if ((worldDir.y < 0.0 && eyeAltitude < CLOUD_PLANE_ALTITUDE)
// 	 || (worldDir.y > 0.0 && eyeAltitude > CLOUD_PLANE_ALTITUDE)) return vec4(0.0, 0.0, 0.0, 1.0);

// 	vec3 planeOrigin = vec3(0.0, planetRadius + eyeAltitude, 0.0);
// 	vec2 intersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + CLOUD_PLANE_ALTITUDE);
//     float cloudDistance = eyeAltitude > CLOUD_PLANE_ALTITUDE ? intersection.x : intersection.y;

// 	if (cloudDistance <= 0.0) return vec4(0.0, 0.0, 0.0, 1.0);
// 	vec3 cloudPos = worldDir * cloudDistance;

// 	//float cloudDist = length(cloudPos);
// 	if (cloudDistance > 3e5 - 6e4 * wetness) return vec4(0.0, 0.0, 0.0, 1.0);

// 	float LdotV = dot(worldDir, worldLightVector);

// 	vec3 atmos = Atmosphere(worldDir, worldSunVector, 1.0, cloudDistance * 1.2e-4) * 0.5;
// 	//atmos += DoNightEye(Atmosphere(worldDir, -worldSunVector, 1.0, cloudDist * 2e-4) * MoonFlux);
// 	vec4 cloudSample = vec4(0.0, 0.0, 0.0, 1.0);
// 	cloudTransmittance = 1.0;

// 	#ifdef CLOUDS_WEATHER
// 		vec2 weatherMap = texelFetch(noisetex, ivec2(worldDay) % noiseTextureResolution, 0).yz;
// 	#endif

// 	#ifdef CIRROCUMULUS_CLOUDS
// 		#ifdef CLOUDS_WEATHER
// 			if (weatherMap.x > 0.43)
// 		#endif
// 		{
// 			float atmosFade = fastExp(-cloudDistance * fma(0.02, wetness, 0.12) * rcp(float(CLOUD_PLANE_ALTITUDE)));
// 			vec4 sampleTemp = PlanarSample1(cloudDistance, cameraPosition.xz + cloudPos.xz, LdotV, 0.5, phases, worldDir);
// 			sampleTemp.rgb += atmos * sampleTemp.a;
// 			cloudTransmittance *= 1.0 - sampleTemp.a;

// 			sampleTemp *= atmosFade;
// 			cloudSample.a *= 1.0 - sampleTemp.a;
// 			cloudSample.rgb += sampleTemp.rgb;
// 			//color.rgb = color.rgb * oneMinus(sampleTemp.a) + sampleTemp.rgb;
// 			//color.rgb = mix(color.rgb, sampleTemp.rgb, saturate(sampleTemp.a));
// 		}
// 	#endif
// 	#if CIRRUS_CLOUDS > 0
// 		#ifdef CLOUDS_WEATHER
// 			if (weatherMap.y < 0.45)
// 		#endif
// 		{
// 			float atmosFade = fastExp(-cloudDistance * fma(0.02, wetness, 0.12) * rcp(float(CLOUD_PLANE_ALTITUDE)));
// 			vec4 sampleTemp = PlanarSample0(cloudDistance, cameraPosition.xz + cloudPos.xz, LdotV);
// 			sampleTemp.rgb += atmos * sampleTemp.a;
// 			cloudTransmittance *= 1.0 - sampleTemp.a;

// 			sampleTemp *= atmosFade;
// 			cloudSample.a *= 1.0 - sampleTemp.a;
// 			cloudSample.rgb += sampleTemp.rgb;
// 			//color.rgb = color.rgb * oneMinus(sampleTemp.a) + sampleTemp.rgb;
// 			//color.rgb = mix(color.rgb, sampleTemp.rgb, saturate(sampleTemp.a));
// 			//cloudTransmittance *= 1.0 - cloudSample1.a;
// 		}
// 	#endif

// 	return cloudSample;
// }
