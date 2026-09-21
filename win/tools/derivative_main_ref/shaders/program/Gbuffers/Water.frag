
layout(location = 0) out vec3 colortex7Out;
layout(location = 1) out vec4 reflectionData;
layout(location = 2) out vec4 colortex3Out;

/* DRAWBUFFERS:723 */

#include "/lib/Head/Common.inc"

uniform sampler2D tex;
#ifdef MC_NORMAL_MAP
    uniform sampler2D normals;
#endif

#include "/lib/Head/Uniforms.inc"

in vec2 texcoord;
in vec2 lightmap;

in vec3 minecraftPos;

in vec4 tint;
in vec4 viewPos;
//in vec3 worldNormal;

flat in mat3 tbnMatrix;

flat in uint materialIDs;

#include "/lib/Atmosphere/Atmosphere.glsl"

#define PHYSICS_OCEAN_SUPPORT

#ifdef PHYSICS_OCEAN
	#define PHYSICS_FRAGMENT
	#include "/lib/Water/PhysicsOceans.glsl"
#endif

#include "/lib/Water/WaterWave.glsl"

vec2 GetWaterParallaxCoord(in vec3 position, in vec3 tangentViewVector) {
	vec3 stepSize = tangentViewVector * vec3(vec2(0.1 * WATER_WAVE_HEIGHT), 1.0);
    stepSize *= 0.02 / abs(stepSize.z);

    vec3 samplePos = vec3(position.xz - position.y, 1.0) + stepSize;
	float sampleHeight = WaterHeight(samplePos.xy);

	for (uint i = 0u; sampleHeight < samplePos.z && i < 60u; ++i) {
        samplePos += stepSize;
		sampleHeight = WaterHeight(samplePos.xy);
	}

	return samplePos.xy;
}

//#include "/lib/Surface/ManualTBN.glsl"

#include "/lib/Surface/RainEffect.glsl"

#include "/lib/Head/Functions.inc"

#include "/lib/Head/Material.inc"

#include "/lib/Surface/ScreenSpaceReflections.glsl"

vec4 CalculateSpecularReflections(in vec3 normal, in float skylight, in vec3 viewPos) {
	skylight = smoothstep(0.3, 0.8, skylight);
	vec3 viewDir = normalize(viewPos);

	vec3 rayDir = reflect(viewDir, normal);

	float NdotL = dot(normal, rayDir);
	if (NdotL < 1e-6) return vec4(0.0);

	//float dither = BlueNoiseTemporal(0.447213595);
	float dither = InterleavedGradientNoiseTemporal(gl_FragCoord.xy);
	vec3 screenPos = vec3(gl_FragCoord.xy * screenPixelSize, gl_FragCoord.z);
	
	float NdotV = max(1e-6, dot(normal, -viewDir));
	//#ifdef HQ_TRACING
		bool hit = ScreenSpaceRayTrace(viewPos, rayDir, dither, RAYTRACE_SAMPLES, screenPos);
	//#else
	//	bool hit = ScreenSpaceRayTrace(viewPos, rayDir, dither, RAYTRACE_SAMPLES, screenPos);
	//#endif

	vec3 reflection;
	if (materialIDs == 17u) {
		#ifdef REAL_SKY_REFLECTION
			if (isEyeInWater == 0) {
				if (skylight > 1e-3) {
					vec3 rayDirWorld = mat3(gbufferModelViewInverse) * rayDir;
					float NdotU = saturate((dot(normal, gbufferModelView[1].xyz) + 0.7) * 2.0) * 0.75 + 0.25;
					//float tileSize = floor(min(viewWidth * rcp(3.0), viewHeight * 0.5)) * 0.25;
					vec4 skyboxData = textureBicubic(colortex5, ProjectSky(rayDirWorld) + vec2(0.0, skyCaptureRes.y * screenPixelSize.y));

					vec3 sunmoon = RenderSunReflection(rayDirWorld, worldSunVector);
					sunmoon += RenderMoonReflection(rayDirWorld, worldSunVector);

					reflection = hit ? texelFetch(colortex4, ivec2(screenPos.xy), 0).rgb : skyboxData.rgb * skylight * NdotU;
					reflection += sunmoon * skyboxData.a * skylight * AtmosphereAbsorption(rayDirWorld, AtmosphereExtent);
				}
			} else {
				reflection = hit ? texelFetch(colortex4, ivec2(screenPos.xy), 0).rgb : vec3(0.05, 0.7, 1.0) * 0.3;
			}
		#else
			if (hit) {
				reflection = texelFetch(colortex4, ivec2(screenPos.xy), 0).rgb;
			}
			#if defined IS_OVERWORLD
				else if (skylight > 1e-3) {
					if (isEyeInWater == 0) {
						vec3 rayDirWorld = mat3(gbufferModelViewInverse) * rayDir;
						float NdotU = saturate((dot(normal, gbufferModelView[1].xyz) + 0.7) * 2.0) * 0.75 + 0.25;
						//float tileSize = floor(min(viewWidth * rcp(3.0), viewHeight * 0.5)) * 0.25;
						vec4 skyboxData = textureBicubic(colortex5, ProjectSky(rayDirWorld) + vec2(0.0, skyCaptureRes.y * screenPixelSize.y));

						// vec3 sunmoon = RenderSunReflection(rayDirWorld, worldSunVector);
						// sunmoon += RenderMoonReflection(rayDirWorld, worldSunVector);

						// skyboxData.rgb += sunmoon * skyboxData.a * AtmosphereAbsorption(rayDirWorld, AtmosphereExtent);
						reflection = skyboxData.rgb * skylight * NdotU;
					} else {
						reflection = vec3(0.05, 0.7, 1.0) * 0.25 * (timeNoon + timeMidnight * NIGHT_BRIGHTNESS);
					}
				}
			#elif defined IS_END
				else {
					if (isEyeInWater == 0) {
						vec3 rayDirWorld = mat3(gbufferModelViewInverse) * rayDir;
						float NdotU = saturate((dot(normal, gbufferModelView[1].xyz) + 0.7) * 2.0) * 0.75 + 0.25;
						#ifdef DARK_END
							bool darkEnd = bossBattle == 2 || bossBattle == 3;
						#else
							const bool darkEnd = false;
						#endif
						vec3 sunDisc = RenderSun(rayDirWorld, worldSunVector);
						sunDisc *= vec3(0.99, 0.93, 0.65) * 0.1;
						reflection = mix(vec3(0.396, 0.352, 0.108), vec3(0.04, 0.02, 0.05), float(darkEnd) * 0.9) * 2.0 * exp2(-max0(rayDirWorld.y) * 1.5);	
						if (!darkEnd) reflection += sunDisc;
						reflection *= NdotU;
					} else {
						reflection = vec3(0.05, 0.7, 1.0) * 0.25;
					}
				}
			#endif
		#endif
	} else {
		if (hit) {
			reflection = texelFetch(colortex4, ivec2(screenPos.xy), 0).rgb;
		}
		#if defined IS_OVERWORLD
			else if (skylight > 1e-3) {
				vec3 rayDirWorld = mat3(gbufferModelViewInverse) * rayDir;
				float NdotU = saturate((dot(normal, gbufferModelView[1].xyz) + 0.7) * 2.0) * 0.75 + 0.25;
				//float tileSize = floor(min(viewWidth * rcp(3.0), viewHeight * 0.5)) * 0.25;
				vec4 skyboxData = textureBicubic(colortex5, ProjectSky(rayDirWorld) + vec2(0.0, skyCaptureRes.y * screenPixelSize.y));

				// vec3 sunmoon = RenderSun(rayDirWorld, worldSunVector);
				// sunmoon += RenderMoonReflection(rayDirWorld, worldSunVector);

				// skyboxData.rgb += sunmoon * skyboxData.a * AtmosphereAbsorption(rayDirWorld, AtmosphereExtent);
				reflection = skyboxData.rgb * skylight * NdotU;
			}
		#elif defined IS_END
			else {
				vec3 rayDirWorld = mat3(gbufferModelViewInverse) * rayDir;
				float NdotU = saturate((dot(normal, gbufferModelView[1].xyz) + 0.7) * 2.0) * 0.75 + 0.25;
				#ifdef DARK_END
					bool darkEnd = bossBattle == 2 || bossBattle == 3;
				#else
					const bool darkEnd = false;
				#endif
				vec3 sunDisc = RenderSun(rayDirWorld, worldSunVector);
				sunDisc *= vec3(0.99, 0.93, 0.65) * 0.1;
				reflection = mix(vec3(0.396, 0.352, 0.108), vec3(0.04, 0.02, 0.05), float(darkEnd) * 0.9) * 2.0 * exp2(-max0(rayDirWorld.y) * 1.5);	
				if (!darkEnd) reflection += sunDisc;
				reflection *= NdotU;
			}
		#endif
	}

	float specular;
	if (isEyeInWater == 1) { // 全反射
		//specular = FresnelDielectricN(NdotV, 1.000293 / WATER_REFRACT_IOR);
		specular = FresnelDielectricN(NdotV, 1.0 / WATER_REFRACT_IOR);
	}else{
		specular = FresnelDielectricN(NdotV, materialIDs == 17u ? WATER_REFRACT_IOR : GLASS_REFRACT_IOR);
	}

	return clamp16F(vec4(reflection * specular, 1.0 - specular));
}

void main() {
	vec4 albedo = texture(tex, texcoord) * tint;

    //mat3 tbnMatrix = manualTBN(viewPos.xyz, texcoord);

	vec3 normalData;
	#ifdef PHYSICS_OCEAN
		if (materialIDs == 17u) {
				WavePixelData wave = physics_wavePixel(physics_localPosition.xz, physics_localWaviness, physics_iterationsNormal, physics_gameTime);
				normalData = mat3(gbufferModelView) * wave.normal;
				normalData = isEyeInWater == 1 ? -normalData : normalData;
		} else {
			#ifdef MC_NORMAL_MAP
				normalData = texture(normals, texcoord).rgb;
				DecodeNormalTex(normalData);
			#else
				normalData = vec3(0.0, 0.0, 1.0);
			#endif

			#if defined IS_OVERWORLD
				#ifdef RAIN_SPLASH_EFFECT
					if (wetnessCustom > 1e-2) {
						vec2 rainNormal = GetRainNormal(wetnessCustom, minecraftPos);
						normalData.xy += rainNormal * wetnessCustom * saturate(lightmap.y * 10.0 - 9.0);
					}
				#endif
			#endif

			normalData = normalize(tbnMatrix * normalData);
		}
	#else
		if (materialIDs == 17u) {
			#ifdef WATER_PARALLAX
				vec2 position = GetWaterParallaxCoord(minecraftPos, normalize(viewPos.xyz * tbnMatrix));
				normalData = GetWavesNormal(position);
			#else
				normalData = GetWavesNormal(minecraftPos.xz - minecraftPos.y);
			#endif
		} else {
			#ifdef MC_NORMAL_MAP
				normalData = texture(normals, texcoord).rgb;
				DecodeNormalTex(normalData);
			#else
				normalData = vec3(0.0, 0.0, 1.0);
			#endif
		}

		#if defined IS_OVERWORLD
			#ifdef RAIN_SPLASH_EFFECT
				if (wetnessCustom > 1e-2) {
					vec2 rainNormal = GetRainNormal(minecraftPos);
					normalData.xy += rainNormal * wetnessCustom * saturate(lightmap.y * 10.0 - 9.0);
				}
			#endif
		#endif

		normalData = normalize(tbnMatrix * normalData);
	#endif

	colortex7Out.xy = lightmap + (bayer4(gl_FragCoord.xy) - 0.5) * rcp(255.0);
	colortex7Out.z = float(materialIDs + 0.1) * rcp(255.0);

	colortex3Out.xy = EncodeNormal(normalData);
	colortex3Out.z = PackUnorm2x8(albedo.rg);
	colortex3Out.w = PackUnorm2x8(albedo.ba);

	reflectionData = CalculateSpecularReflections(normalData, cube(lightmap.g), viewPos.xyz);
}
