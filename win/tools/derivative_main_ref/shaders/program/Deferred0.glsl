
#define DEFERRED0
#define PRECOMPUTED_ATMOSPHERIC_SCATTERING

layout(location = 0) out vec3 transmittanceOut;
layout(location = 1) out vec3 colortex5Out;

/* DRAWBUFFERS:45 */

//in vec2 screenCoord;

flat in vec3 directIlluminance;
flat in vec3 skyIlluminance;

flat in vec3 sunIlluminance;
flat in vec3 moonIlluminance;

#include "/lib/Head/Common.inc"

//----------------------------------------------------------------------------//

uniform sampler2D noisetex;
uniform sampler3D colortex4;

uniform float nightVision;
uniform float viewWidth;
uniform float viewHeight;
uniform float wetness;
uniform float eyeAltitude;
uniform float isLightningFlashing;
uniform float worldTimeCounter;

uniform vec3 cameraPosition;
uniform vec3 worldSunVector;
uniform vec3 worldLightVector;

uniform int frameCounter;
uniform int moonPhase;

uniform vec2 screenPixelSize;
uniform vec2 screenSize;

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Noise.inc"

#include "/lib/Atmosphere/Atmosphere.glsl"

#include "/lib/Atmosphere/VolumetricClouds.glsl"

#include "/lib/Atmosphere/PlanarClouds.glsl"

void GetPlanetCurvePosition(inout vec3 p) {
    p.y = length(p + vec3(0.0, planetRadius, 0.0)) - planetRadius;
}

vec3 RenderSkybox(in vec3 worldDir, in CloudProperties cloudProperties) {
	vec4 cloudsData = vec4(0.0, 0.0, 0.0, 1.0);
	vec3 transmittance;
	vec3 skyRadiance = GetSkyRadiance(atmosphereModel, worldDir, worldSunVector, transmittance) * 20.0;

	float LdotV = dot(worldDir, worldLightVector);

	vec4 phases;	/* forwardsLobe */										/* backwardsLobe */																	/* forwardsPeak */
	phases.x = 	HenyeyGreensteinPhase(LdotV, cloudForwardG) 	  * 0.7  + HenyeyGreensteinPhase(LdotV, cloudBackwardG)		  * cloudBackwardWeight  	  + CornetteShanksPhase(LdotV, 0.9) * cloudProperties.cloudPeakWeight;
	phases.y = 	HenyeyGreensteinPhase(LdotV, cloudForwardG * 0.7) * 0.35 + HenyeyGreensteinPhase(LdotV, cloudBackwardG * 0.7) * cloudBackwardWeight * 0.6 + CornetteShanksPhase(LdotV, 0.6) * cloudProperties.cloudPeakWeight * 0.5;
	phases.z = 	HenyeyGreensteinPhase(LdotV, cloudForwardG * 0.5) * 0.17 + HenyeyGreensteinPhase(LdotV, cloudBackwardG * 0.5) * cloudBackwardWeight * 0.3 + CornetteShanksPhase(LdotV, 0.4) * cloudProperties.cloudPeakWeight * 0.2;
	phases.w = 	HenyeyGreensteinPhase(LdotV, cloudForwardG * 0.3) * 0.08 + HenyeyGreensteinPhase(LdotV, cloudBackwardG * 0.3) * cloudBackwardWeight * 0.2 + CornetteShanksPhase(LdotV, 0.2) * cloudProperties.cloudPeakWeight * 0.1;
	//phases.x = MiePhaseClouds(LdotV, vec3(cloudForwardG, cloudBackwardG, 0.9), vec3(0.7, cloudBackwardWeight, cloudPeakWeight));
	//phases.y = MiePhaseClouds(LdotV, vec3(cloudForwardG, cloudBackwardG, 0.9) * 0.7, vec3(0.35, cloudBackwardWeight * 0.6, cloudPeakWeight * 0.5));
	//phases.z = MiePhaseClouds(LdotV, vec3(cloudForwardG, cloudBackwardG, 0.9) * 0.5, vec3(0.17, cloudBackwardWeight * 0.3, cloudPeakWeight * 0.2));
	//phases.w = MiePhaseClouds(LdotV, vec3(cloudForwardG, cloudBackwardG, 0.9) * 0.3, vec3(0.08, cloudBackwardWeight * 0.15, cloudPeakWeight * 0.1));

	vec3 planeOrigin = vec3(0.0, planetRadius + eyeAltitude, 0.0);
    bool intersectsGround = RaySphereIntersection(planeOrigin, worldDir, planetRadius).y >= 0.0;

	#ifdef VOLUMETRIC_CLOUDS
		if ((worldDir.y > 0.0 && eyeAltitude < cloudProperties.altitude)
		|| (intersectsGround && eyeAltitude > cloudProperties.maxAltitude)) {

			vec2 bottomIntersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + cloudProperties.altitude);
			vec2 topIntersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + cloudProperties.maxAltitude);

			float startLength, endLength;
			if (eyeAltitude > cloudProperties.maxAltitude) {
				startLength = topIntersection.x;
				endLength = bottomIntersection.x;
			} else {
				startLength = bottomIntersection.y;
				endLength = topIntersection.y;
			}

			// The range of eye in cloudsData
			float rayRange = oneMinus(saturate((eyeAltitude - cloudProperties.maxAltitude) * 0.1)) *
							oneMinus(saturate((cloudProperties.altitude - eyeAltitude) * 0.1));

			// The ray distance in range
			float rayDist = bottomIntersection.y >= 0.0 && eyeAltitude > cloudProperties.altitude ? bottomIntersection.x : topIntersection.y;
			//rayDist = min(rayDist, cloudProperties.altitude * 10.0);

			startLength *= oneMinus(rayRange);
			endLength = mix(endLength, rayDist, rayRange);

			uint raySteps = CLOUD_CUMULUS_SAMPLES / 2;
			raySteps = uint(mix(raySteps, uint(raySteps * 0.6), abs(worldDir.y))); // Steps Fade

			float rayLength = clamp(endLength - startLength, 0.0, 2e4) * rcp(raySteps);
			vec3 rayStep = rayLength * worldDir;
            GetPlanetCurvePosition(rayStep);
			vec3 rayPos = (startLength + rayLength * 0.5) * worldDir + cameraPosition;
            GetPlanetCurvePosition(rayPos);

			float noiseDetail = GetNoiseDetail(worldDir);
			//float lightNoise = InterleavedGradientNoise();

			//uint raySteps = max(uint(CLOUD_CUMULUS_SAMPLES - sqrt(dist) * 0.06), CLOUD_CUMULUS_SAMPLES / 2);
			//uint raySteps = uint(CLOUD_CUMULUS_SAMPLES);

			float scatteringSun = 0.0;
			float scatteringSky = 0.0;

			//float powderIntensity = saturate(CornetteShanksPhase(LdotV, 0.5));
			//float powderIntensity = 0.8 * sqr(dot(worldDir, worldLightVector) * 0.5 + 0.5);
			float transmittance = 1.0;

			for (uint i = 0u; i < raySteps; ++i, rayPos += rayStep) {
				if (transmittance < minTransmittance) break;
				if (rayPos.y < cloudProperties.altitude || rayPos.y > cloudProperties.maxAltitude) continue;

				float dist = distance(rayPos, cameraPosition);
				if (dist > planetRadius + cloudProperties.maxAltitude) continue;

				float density = CloudVolumeDensity(cloudProperties, rayPos, 5u, mix(noiseDetail, 1.0, exp2(-dist * 0.001f)));
				if (density < 1e-4) continue;

				float sunlightOD = CloudVolumeSunLightOD(cloudProperties, rayPos, 0.5);

				float powder = oneMinus(fastExp(-density * 36.0)) * 0.82;
				powder /= 1.0 - powder;

				float sunlightEnergy = 	fastExp(-sunlightOD * 2.0) * phases.x;
				sunlightEnergy += 		fastExp(-sunlightOD * 0.8) * phases.y;
				sunlightEnergy += 		fastExp(-sunlightOD * 0.3) * phases.z;
				sunlightEnergy += 		fastExp(-sunlightOD * 0.1) * phases.w;
				// float sunlightEnergy = 	rcp(sunlightOD * 2.0 + 1.0) * phases.x;
				// sunlightEnergy += 		rcp(sunlightOD * 0.9 + 1.0) * phases.y;
				// sunlightEnergy += 		rcp(sunlightOD * 0.4 + 1.0) * phases.z;
				// sunlightEnergy += 		rcp(sunlightOD * 0.2 + 1.0) * phases.w;

				float skylightEnergy = CloudVolumeSkyLightOD(cloudProperties, rayPos, 0.5);
				skylightEnergy = fastExp(-skylightEnergy) + fastExp(-skylightEnergy * 0.1) * 0.1;

				float stepTransmittance = fastExp(-density * 0.12 * rayLength);
				float cloudsTemp = powder * transmittance * oneMinus(stepTransmittance);
				scatteringSun += sunlightEnergy * cloudsTemp;
				scatteringSky += skylightEnergy * cloudsTemp;
				transmittance *= stepTransmittance;	
			}

			if (transmittance < 1.0 - minTransmittance) {
				bool moonlit = worldSunVector.y < -0.04;
				vec3 scattering = scatteringSun * 22.0 * cloudProperties.sunlighting * (moonlit ? moonIlluminance : sunIlluminance);
				scattering += scatteringSky * 0.15 * cloudProperties.skylighting * skyIlluminance;

				// lightning
				if (isLightningFlashing > 1e-2) scattering += sqr(scatteringSky) * 0.1 * lightningColor;

				#ifdef AURORA
					scattering += sqr(scatteringSky) * vec3(0.0, 0.001, 0.0005) * auroraAmount;
				#endif

				rayPos -= cameraPosition;
				#ifdef FULL_AERIAL_PERSPECTIVE
					vec3 airTransmittance;
					vec3 aerialPerspective = 20.0 * GetSkyRadianceToPoint(atmosphereModel, rayPos, worldSunVector, airTransmittance);
					scattering *= airTransmittance;
					scattering += aerialPerspective * oneMinus(transmittance);
					float atmosFade = fastExp(-length(rayPos) * (0.1 + 0.1 * wetness) * 1e-4);
				#else
					float atmosFade = fastExp(-length(rayPos) * (0.2 + 0.1 * wetness) * 1e-4);
				#endif
				scattering = scattering * atmosFade + skyRadiance * oneMinus(transmittance) * oneMinus(atmosFade);

				cloudsData = vec4(scattering, transmittance);
			}
		}
	#endif

	#ifdef PLANAR_CLOUDS
		//vec4 cloudsTemp = PlanarClouds(worldDir, dither, phases, transmittanceTemp);

		if ((worldDir.y > 0.0 && eyeAltitude < CLOUD_PLANE_ALTITUDE)
		|| (intersectsGround && eyeAltitude > CLOUD_PLANE_ALTITUDE)) {
			vec2 intersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + CLOUD_PLANE_ALTITUDE);
			float cloudDistance = eyeAltitude > CLOUD_PLANE_ALTITUDE ? intersection.x : intersection.y;

			if (cloudDistance > 0.0 && cloudDistance < planetRadius + CLOUD_PLANE_ALTITUDE) {
				vec3 cloudPos = worldDir * cloudDistance;

				#ifdef FULL_AERIAL_PERSPECTIVE
					vec3 airTransmittance;
					vec3 aerialPerspective = 20.0 * GetSkyRadianceToPoint(atmosphereModel, cloudPos, worldSunVector, airTransmittance);
				#endif
				cloudPos += cameraPosition;

				vec4 cloudsTemp = vec4(0.0, 0.0, 0.0, 1.0);

				#ifdef CIRROCUMULUS_CLOUDS
					#ifdef CLOUDS_WEATHER
						if (cloudDynamicWeather.x < 0.4)
					#endif
					{
						vec4 sampleTemp = PlanarSample1(cloudDistance, cloudPos.xz, LdotV, 0.5, phases, worldDir);

						if (sampleTemp.a > minTransmittance) {
							#ifdef FULL_AERIAL_PERSPECTIVE
								float atmosFade = fastExp(-cloudDistance * fma(0.025, wetness, 0.05) * 0.00015);
								sampleTemp.rgb *= airTransmittance;
								sampleTemp.rgb += aerialPerspective * sampleTemp.a;
							#else
								float atmosFade = fastExp(-cloudDistance * fma(0.05, wetness, 0.1) * 0.00015);
							// 	sampleTemp.rgb += skyRadiance * sampleTemp.a * oneMinus(airTransmittance);
							#endif
							sampleTemp.rgb = sampleTemp.rgb * atmosFade + skyRadiance * sampleTemp.a * oneMinus(atmosFade);
						}

						cloudsTemp.rgb = sampleTemp.rgb;
						cloudsTemp.a -= sampleTemp.a;
					}
				#endif
				#if CIRRUS_CLOUDS > 0
					#ifdef CLOUDS_WEATHER
						if (cloudDynamicWeather.y < 0.5)
					#endif
					{
						vec4 sampleTemp = PlanarSample0(cloudDistance, cloudPos.xz, LdotV);

						if (sampleTemp.a > minTransmittance) {
							#ifdef FULL_AERIAL_PERSPECTIVE
								float atmosFade = fastExp(-cloudDistance * fma(0.025, wetness, 0.05) * 0.00015);
								sampleTemp.rgb *= airTransmittance;
								sampleTemp.rgb += aerialPerspective * sampleTemp.a;
							#else
								float atmosFade = fastExp(-cloudDistance * fma(0.05, wetness, 0.1) * 0.00015);
							// 	sampleTemp.rgb += skyRadiance * sampleTemp.a * oneMinus(airTransmittance);
							#endif
							sampleTemp.rgb = sampleTemp.rgb * atmosFade + skyRadiance * sampleTemp.a * oneMinus(atmosFade);
						}

						cloudsTemp.rgb += sampleTemp.rgb * cloudsTemp.a;
						cloudsTemp.a *= 1.0 - sampleTemp.a;
					}
				#endif

				if (eyeAltitude < CLOUD_PLANE_ALTITUDE) {
					cloudsData.rgb += cloudsTemp.rgb * cloudsData.a;
				} else {
					cloudsData.rgb = cloudsData.rgb * cloudsTemp.a + cloudsTemp.rgb;
				}

				cloudsData.a *= cloudsTemp.a;
			}
		}
	#endif

	vec3 skyboxData = skyRadiance * cloudsData.a + cloudsData.rgb;

	// #ifdef AURORA
	// 	if (auroraAmount > 1e-2) skyboxData.rgb += NightAurora(worldDir) * cloudsData.a;
	// #endif

	vec3 sunmoon = RenderSun(worldDir, worldSunVector);
	sunmoon += RenderMoonReflection(worldDir, worldSunVector);

	skyboxData += sunmoon * remap(minTransmittance, 1.0, cloudsData.a) * transmittance;

	return clamp16F(skyboxData);
}

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	if (int(gl_FragCoord.x) == skyCaptureRes.x) {
		switch (int(gl_FragCoord.y)) {
		case 0:
			colortex5Out = directIlluminance;
			break;

		case 1:
			colortex5Out = skyIlluminance;
			break;

		case 2:
			colortex5Out = sunIlluminance;
			break;

		case 3:
			colortex5Out = moonIlluminance;
			break;

		#ifdef CLOUDS_WEATHER
			case 5:
			colortex5Out = cloudDynamicWeather;
			break;
		#endif
		}
	} else if (gl_FragCoord.y < skyCaptureRes.y + 2.0) {
		// Raw sky map

		vec3 worldDir = UnprojectSky(gl_FragCoord.xy * rcp(skyCaptureRes));
		colortex5Out = GetSkyRadiance(atmosphereModel, worldDir, worldSunVector, transmittanceOut) * 20.0;
	} else {
		// Sky map with clouds

		vec3 worldDir = UnprojectSky(gl_FragCoord.xy * rcp(skyCaptureRes) - vec2(0.0, 1.0));
		colortex5Out = RenderSkybox(worldDir, GetGlobalCloudProperties());
	}
}
