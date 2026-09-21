
#define EMISSION_MODE 0 // [0 1 2]
#define EMISSION_BRIGHTNESS 1.0 // [0.0 0.1 0.2 0.3 0.5 0.7 1.0 1.5 2.0 3.0 5.0 7.0 10.0 15.0 20.0 30.0 50.0 70.0 100.0]

#define EMISSIVE_ORES

float lightSourceMask = 1.0;
float albedoLuminance = length(albedo);

GetBlocklightFalloff(mcLightmap.r);

#if EMISSION_MODE < 2
	vec3 EmissionColor = vec3(0.0);

	switch (materialID) {
	// Total glowing
		case 20: case 36:
			EmissionColor += albedoLuminance;
			lightSourceMask = 0.1;
			break;
	// Torch like
		case 21:
			EmissionColor += 4.0 * blocklightColor * float(albedoRaw.r > 0.8 || albedoRaw.r > albedoRaw.g * 1.4);
			lightSourceMask = 0.15;
			break;
	// Fire
		case 22: case 15:
			EmissionColor += 6.0 * blocklightColor * cube(albedoLuminance);
			lightSourceMask = 0.1;
			break;
	// Glowstone like
		case 23:
			EmissionColor += 2.5 * blocklightColor * cube(albedoLuminance);
			lightSourceMask = 0.15;
			break;
	// Sea lantern like
		 case 24:
			EmissionColor += 2.0 * cube(albedoLuminance);
			lightSourceMask = 0.0;
			break;
	// Redstone
		case 25:
			if (fract(worldPos.y + cameraPosition.y) > 0.18) EmissionColor += step(0.65, albedoRaw.r);
			else EmissionColor += step(1.25, albedo.r / (albedo.g + albedo.b)) * step(0.5, albedoRaw.r);
			EmissionColor *= vec3(2.1, 0.9, 0.9);
			break;
	// Soul fire
		case 26:
			EmissionColor += (albedoLuminance + 0.6) * step(0.53, albedoRaw.b);
			lightSourceMask = 0.5;
			break;
	// Amethyst
		case 27:
			EmissionColor += min(mcLightmap.r * 2e2 + 0.05, 2.0) * pow(albedoLuminance, min(mcLightmap.r * 1e2, 2.5));
			break;
	// Glowberry
		case 28:
			EmissionColor += saturate(dot(saturate(albedo - 0.1), vec3(1.0, -0.6, -0.99))) * vec3(28.0, 25.0, 21.0);
			lightSourceMask = 0.4;
			break;
	// Rails
		case 29:
			EmissionColor += vec3(2.1, 0.9, 0.9) * albedoLuminance * step(albedoRaw.g * 2.0 + albedoRaw.b, albedoRaw.r);
			break;
	// Beacon core
		case 30:
    		vec3 midBlockPos = abs(fract(worldPos + cameraPosition) - 0.5);
			if (maxOf(midBlockPos) < 0.4 && albedo.b > 0.5) EmissionColor += 6.0 * albedoLuminance;
			lightSourceMask = 0.2;
			break;
	// Sculk
		case 31:
			EmissionColor += 0.04 * sqr(albedoLuminance) * float((albedoRaw.b * 2.0 > albedoRaw.r + albedoRaw.g) && albedoRaw.b > 0.55);
			break;
	// Glow lichen
		case 32:
			if (albedoRaw.r > albedoRaw.b * 1.2) EmissionColor += 3.0;
			else EmissionColor += albedoLuminance * 0.1;
			break;
	// Partial glowing
		case 33:
			EmissionColor += 30.0 * albedoLuminance * cube(saturate(albedo - 0.5));
			lightSourceMask = 0.5;
			break;
	// Middle glowing
		case 34:
    		vec2 midBlockPosXZ = abs(fract(worldPos.xz + cameraPosition.xz) - 0.5);
			EmissionColor += step(maxOf(midBlockPosXZ), 0.063) * albedoLuminance;
	}

	//lightSourceMask = 0.9 * lightSourceMask + 0.1;

	sceneData += EmissionColor * TORCHLIGHT_BRIGHTNESS;
#endif

#if EMISSION_MODE > 0
	sceneData += material.emissiveness * 1.5 * EMISSION_BRIGHTNESS;
#endif

#ifdef EMISSIVE_ORES
	if (materialID == 57) {
		float isOre = saturate((max(max(dot(albedoRaw, vec3(2.0, -1.0, -1.0)), dot(albedoRaw, vec3(-1.0, 2.0, -1.0))), dot(albedoRaw, vec3(-1.0, -1.0, 2.0))) - 0.1) * rcp(0.3));
		sceneData += LinearToSRGB(isOre * (pow5(max0(albedoRaw - vec3(0.1))))) * 2.0;
	}
	if (materialID == 58) {
		float isNetherOre = saturate(dot(albedoRaw, vec3(-20.0, 30.0, 10.0)));
		sceneData += LinearToSRGB(isNetherOre * (cube(max0(albedoRaw - vec3(0.1))))) * 2.0;
	}
#endif

#if defined IS_NETHER
	if (mcLightmap.r > 1e-5) sceneData += mcLightmap.r * (ao * oneMinus(mcLightmap.r) + mcLightmap.r) * 20.0 * blocklightColor * TORCHLIGHT_BRIGHTNESS * lightSourceMask * metalMask;
#else
	if (mcLightmap.r > 1e-5) sceneData += mcLightmap.r * (ao * oneMinus(mcLightmap.r) + mcLightmap.r) * 2.0 * blocklightColor * TORCHLIGHT_BRIGHTNESS * lightSourceMask;
#endif

#ifdef HELD_TORCHLIGHT
	if (heldBlockLightValue + heldBlockLightValue2 > 1e-3) {
		float falloff = rcp(dotSelf(worldPos) + 1.0);
		falloff *= fma(NdotV, 0.8, 0.2);

		#if defined IS_NETHER
			sceneData += falloff * (ao * oneMinus(falloff) + falloff) * 2.0 * max(heldBlockLightValue, heldBlockLightValue2) * HELDLIGHT_BRIGHTNESS * blocklightColor * metalMask;
		#else
			sceneData += falloff * (ao * oneMinus(falloff) + falloff) * 0.2 * max(heldBlockLightValue, heldBlockLightValue2) * HELDLIGHT_BRIGHTNESS * blocklightColor;
		#endif
	}
#endif

sceneData += float(materialID == 12) * 12.0 + float(materialID == 36) * 2.0 + float(materialID == 19) * albedoLuminance * 2e2;
