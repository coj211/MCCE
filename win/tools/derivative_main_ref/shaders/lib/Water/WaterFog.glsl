
void WaterFog(inout vec3 color, in TranslucentMask mask, in float waterSkylight, in float LdotV, in float waterDepth) {
	float fogDensity = mix(WATER_FOG_DENSITY * fma(0.1, wetnessCustom * eyeSkylightFix, 0.16), 0.5, mask.ice) * waterDepth;

	#if defined IS_OVERWORLD
		vec3 waterFogColor = mix(skyIlluminance * 0.4, vec3(GetLuminance(skyIlluminance) * 0.1), 0.8 * wetnessCustom * eyeSkylightFix) * rPI;
		float scatter = HenyeyGreensteinPhase(LdotV, 0.65) + 0.1 * rPI;
		waterFogColor *= 1.0 + 28.0 * oneMinus(wetnessCustom * 0.8) * directIlluminance * scatter;
	#else
		vec3 waterFogColor = vec3(0.035, 0.5, 0.7) * rPI;
	#endif

	vec3 transmittance = fastExp(-(waterAbsorption * 8.0 + 0.03) * fogDensity);

	color *= transmittance;
	color += waterFogColor * waterSkylight * oneMinus(transmittance);
}

void UnderwaterFog(inout vec3 color, in TranslucentMask mask, in float waterDepth) {
	float fogDensity = mix(WATER_FOG_DENSITY * fma(0.05, wetnessCustom * eyeSkylightFix, 0.1), 0.5, mask.ice) * waterDepth;

	#if defined IS_OVERWORLD
		vec3 waterFogColor = mix(skyIlluminance * 0.4, vec3(GetLuminance(skyIlluminance) * 0.1), 0.8 * wetnessCustom * eyeSkylightFix) * rPI;
	#else
		vec3 waterFogColor = vec3(0.035, 0.5, 0.7) * rPI;
	#endif

	vec3 transmittance = fastExp(-(waterAbsorption * 8.0 + 0.03) * max(fogDensity, 2.0) + 0.4);

	color *= transmittance;
	color += waterFogColor * saturate(eyeSkylightFix + 0.2) * oneMinus(transmittance);
}

/*
void WaterFog(inout vec3 color, in TranslucentMask mask, in float waterSkylight, in vec3 viewDir, in float opaqueDepth, in float waterDepth)
{
	if (mask.water + mask.ice + isEyeInWater < 0.5 || isEyeInWater > 1) return;

	if (isEyeInWater == 0) waterDepth = opaqueDepth - waterDepth;		

	float fogDensity = mix(WATER_FOG_DENSITY * fma(0.1, wetnessCustom * eyeSkylightFix, 0.14), 0.7, mask.ice);

	vec3 waterFogColor = vec3(0.05, 0.7, 1.0) * (1.0 + mask.ice);
	waterFogColor = mix(waterFogColor, vec3(0.5), 0.7 * wetnessCustom * eyeSkylightFix);
	waterFogColor *= 0.02 * dot(vec3(0.33333), skyIlluminance);

	vec3 waterSunlightVector = refract(-mat3(gbufferModelView) * worldLightVector, gbufferModelView[1].xyz, 1.0 / WATER_REFRACT_IOR);
	float scatter = 1.0 / (fma(saturate(fma(dot(waterSunlightVector, viewDir), 0.5, 0.5)), 10.0, 0.1));

	if (isEyeInWater == 0) {
		waterFogColor *= waterSkylight;
		waterFogColor += waterFogColor * directIlluminance * 8.0 * scatter;
	}else{
		waterFogColor *= oneMinus(wetnessCustom * 0.4);
		fogDensity *= 0.5;
		vec3 waterSunlightScatter = directIlluminance * scatter * waterFogColor * 2.0;

		float eyeWaterDepth = saturate(eyeBrightnessSmooth.y / 120.0 - 0.8);

		waterFogColor *= fma(dot(viewDir, gbufferModelView[1].xyz), 0.4, 0.6);
		waterFogColor += waterSunlightScatter * eyeWaterDepth;
	}

	float visibility = fastExp(-waterDepth * fogDensity);
		
	visibility = clamp(visibility, 0.35 * mask.ice, 1.0);

	color *= fastExp(-(waterAbsorption * 8.0 + 0.03) * fogDensity * waterDepth + 0.3 + isEyeInWater * 1.2);

	color = mix(waterFogColor * 12.0, color, visibility);
}
*/