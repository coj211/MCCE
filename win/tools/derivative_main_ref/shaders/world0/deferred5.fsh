#version 450 compatibility

#define IS_OVERWORLD


layout(location = 0) out vec2 specularData;
layout(location = 1) out vec3 sceneData;

in vec2 screenCoord;

flat in vec3 directIlluminance;
flat in vec3 skyIlluminance;
flat in vec3 blocklightColor;

flat in vec4 skySHR;
flat in vec4 skySHG;
flat in vec4 skySHB;

//----------------------------------------------------------------------------//

uniform sampler2DShadow shadowtex1;
uniform sampler2D shadowtex0;
uniform sampler2D shadowcolor0;
uniform sampler2D shadowcolor1;

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

#include "/lib/Atmosphere/Atmosphere.glsl"

//----// STRUCTS //-------------------------------------------------------------------------------//

#include "/lib/Head/Mask.inc"
#include "/lib/Head/Material.inc"

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

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

	float CloudShadow(in vec3 worldPos, in CloudProperties cloudProperties) {
		float cloudDensity = 0.0;
		vec3 checkOrigin = worldPos + vec3(0.0, planetRadius, 0.0);
		#ifdef VC_SHADOW
			float checkRadius = planetRadius + cloudProperties.altitude;
			//vec3 checkPos = worldLightVector / abs(worldLightVector.y) * max0(cloudProperties.maxAltitude - abs(worldPos.y)) + worldPos;
			vec3 checkPos = RaySphereIntersection(checkOrigin, worldLightVector, checkRadius + 0.15 * cloudProperties.thickness).y * worldLightVector + worldPos;
			cloudDensity += CloudVolumeDensitySmooth(cloudProperties, checkPos);

			checkPos = RaySphereIntersection(checkOrigin, worldLightVector, checkRadius + 0.5 * cloudProperties.thickness).y * worldLightVector + worldPos;
			cloudDensity += CloudVolumeDensitySmooth(cloudProperties, checkPos);
		#endif
		#ifdef PC_SHADOW
			vec2 checkPos1 = RaySphereIntersection(checkOrigin, worldLightVector, planetRadius + CLOUD_PLANE_ALTITUDE).y * worldLightVector.xz + worldPos.xz;
			cloudDensity += CloudPlanarDensity(checkPos1) * 10.0;
		#endif
		cloudDensity = mix(0.4, cloudDensity, saturate(sqr(abs(worldLightVector.y) * 2.0)));
		// cloudDensity = mix(cloudDensity, 0.9, wetness * 0.5);
		cloudDensity = saturate(cloudDensity);

		return exp2(-cloudDensity * cloudDensity * 2e2);
	}
#endif

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);

	vec3 albedoRaw = texelFetch(colortex6, texel, 0).rgb;
	vec3 albedo = SRGBtoLinear(albedoRaw);

	float depth = GetDepthFix(texel);

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
		// Sky & Clouds
		vec2 skyCaptureCoord = ProjectSky(worldDir);
		sceneData = textureBicubic(colortex5, skyCaptureCoord).rgb;
		#if defined PLANAR_CLOUDS || defined VOLUMETRIC_CLOUDS
			// vec4 cloudData = textureBicubic(colortex1, screenCoord);
			vec4 cloudData = texelFetch(colortex1, texel, 0);
			sceneData = sceneData * cloudData.a + cloudData.rgb;
		#endif

		// Sun & Moon
		vec3 transmittance = texture(colortex4, skyCaptureCoord).rgb;
		if (maxOf(transmittance) > 1e-4) {
			vec3 sunmoon = RenderSun(worldDir, worldSunVector);
			sunmoon += albedo * 2.0 * step(0.06, albedo.g) * (0.2 + nightVision);
			sunmoon += RenderStars(worldDir);

			#if defined PLANAR_CLOUDS || defined VOLUMETRIC_CLOUDS
				sunmoon *= cloudData.a;
			#endif

			sceneData += sunmoon * transmittance;
		}

		specularData = vec2(1.0, 0.0);
	} else {
		sceneData = vec3(0.0);

		vec4 gbuffer3 = texelFetch(colortex3, texel, 0);
		//int materialIDT = int(texelFetch(colortex1, texel, 0).b * 255.0);

		//MaterialMask materialMask = CalculateMasks(materialID);
		bool isGrass = materialID == 6 || materialID == 27 || materialID == 28 || materialID == 33;

		vec2 mcLightmap = gbuffer7.rg;
		mcLightmap.g = isEyeInWater == 1 ? 0.75 : cube(mcLightmap.g);

		vec3 normal = DecodeNormal(gbuffer3.xy);
		vec3 worldNormal = mat3(gbufferModelViewInverse) * normal;

		vec4 specTex = vec4(UnpackUnorm2x8(gbuffer3.z), UnpackUnorm2x8(gbuffer3.w));
		Material material = GetMaterialData(specTex);
		specTex.x = sqr(oneMinus(specTex.x) * oneMinus(wetnessCustom * 0.3));
		specularData = specTex.xy;

		float rawNdotL = dot(worldNormal, worldLightVector);

		// Grass points up
		if (isGrass) worldNormal = vec3(0.0, 1.0, 0.0);

		float opaqueDepth = -viewPos.z;
		//float waterDepth = ScreenToViewSpace(depthT);
		float LdotV = dot(worldLightVector, -worldDir);
		
		//vec3 halfWay = normalize(worldLightVector - worldDir);
		float NdotV = saturate(dot(worldNormal, -worldDir));
		float NdotL = dot(worldNormal, worldLightVector);
		float halfwayNorm = inversesqrt(2.0 * LdotV + 2.0);
		float NdotH = (NdotL + NdotV) * halfwayNorm;
		float LdotH = LdotV * halfwayNorm + halfwayNorm;

		worldPos += gbufferModelViewInverse[3].xyz;

		// Clouds shadow
		float cloudShadow = mix(1.0, 0.03, wetness);

		#ifdef CLOUDS_SHADOW
			CloudProperties cloudProperties = GetGlobalCloudProperties();
			cloudShadow	= max(CloudShadow(worldPos + cameraPosition, cloudProperties), 0.03);
		#endif

		// Sunlight
		vec3 waterTint = isEyeInWater == 1 ? vec3(0.6, 0.9, 1.2) / max(3.0, opaqueDepth * 0.1 * WATER_FOG_DENSITY) : vec3(1.0);	
		vec3 sunlightMult = 64.0 * waterTint * SUNLIGHT_INTENSITY * directIlluminance * cloudShadow;
		vec3 diffuse = vec3(1.0);

		#ifdef TAA_ENABLED
			float dither = BlueNoiseTemporal();
		#else
			float dither = InterleavedGradientNoise(gl_FragCoord.xy);
		#endif

		float distortFactor;
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
			subsurfaceScattering *= eyeSkylightFix;
			sunlightMult *= 1.0 - sssAmount * 0.5;
			sceneData += subsurfaceScattering * sunlightMult;
		}

		vec3 shadow = vec3(0.0);
		vec3 specular = vec3(0.0);
		if (NdotL > 1e-3) {
			float penumbraScale = max(blockerSearch.x / distortFactor, 2.0 / realShadowMapRes);
			if (distanceFade < 1.0) shadow = PercentageCloserFilter(shadowProjPos, dither, penumbraScale);
			shadow = mix(shadow, vec3(1.0), distanceFade);

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
				specular *= SPECULAR_HIGHLIGHT_BRIGHTNESS + wetnessCustom;

				shadow *= saturate(mcLightmap.g * 1e6);
				shadow *= sunlightMult;
			}
		}

		// Skylight
		if (mcLightmap.g > 1e-5) {
			vec3 skylight = FromSH(skySHR, skySHG, skySHB, worldNormal);
			// skylight *= dot(worldNormal, normalize(worldLightVector + vec3(0.0, 1.0, 0.0))) * 3.0 + 4.5;
			skylight *= worldNormal.y * 2.0 + 3.0;

			vec3 skySunLight = (worldNormal.y * 0.24 + 0.4) * directIlluminance;
			//skylight += skySunLight;

			skylight = mix(skylight, skySunLight, wetness * 0.7);

			// lightning
			skylight = skylight * (0.8 - wetness * 0.2) + lightningColor * 1.2;

			#ifdef AURORA
				skylight *= 1.0 + vec3(0.0, 3.0, 1.5) * auroraAmount;
			#endif

			sceneData += skylight * mcLightmap.g * SKYLIGHT_INTENSITY;
		}

		// Basic light
		sceneData += BASIC_BRIGHTNESS + nightVision * 0.1;

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
			if (isEyeInWater == 0) indirectData.rgb *= saturate(mcLightmap.g * 5.0);
			sceneData += indirectData.rgb * GI_BRIGHTNESS * sunlightMult * 0.25;
		#else
			float bounce = CalculateFakeBouncedLight(worldNormal);
			sceneData += bounce * sqr(mcLightmap.g) * sunlightMult;
		#endif

		sceneData *= ao;

		// Block light
		#include "/lib/Lighting/BlockLighting.glsl"

		sceneData += shadow * diffuse;
		sceneData *= albedo;

		if (isEyeInWater == 0) material.isMetal *= 0.2 * smoothstep(0.3, 0.8, mcLightmap.g) + 0.8;
		sceneData *= oneMinus(material.isMetal);
		sceneData += shadow * specular;
	}

	sceneData = clamp16F(sceneData);
}

/* DRAWBUFFERS:04 */