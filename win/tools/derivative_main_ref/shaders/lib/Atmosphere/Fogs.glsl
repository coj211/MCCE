
uniform float blindness;
uniform float darknessFactor;

void CommonFog(inout vec3 color, in float dist) {
	if (blindness + darknessFactor > 0.0) color *= fastExp(-dist * max(blindness, darknessFactor));

	if (isEyeInWater == 2) color = mix(color, vec3(3.96, 0.68, 0.02), saturate(dist));

	if (isEyeInWater == 3) {
		#if defined IS_OVERWORLD
			vec3 fogColor = skyIlluminance + directIlluminance;
			fogColor = 6.0 * mix(fogColor, directIlluminance * 0.1, wetnessCustom * 0.8);
		#else
			vec3 fogColor = vec3(1.76, 1.92, 2.0);
		#endif

		color = mix(fogColor * eyeSkylightFix, color, exp2(-dist * 2.0));
	}
}

#if defined IS_NETHER
	//uniform vec3 fogColor;
	uniform float BiomeNetherWastesSmooth;
	uniform float BiomeWarpedForestSmooth;
	uniform float BiomeCrimsonForestSmooth;
	uniform float BiomeSoulSandValleySmooth;
	uniform float BiomeBasaltDeltasSmooth;

	vec3 NetherFogColor() {
		return vec3(0.99, 0.23, 0.03) 	* BiomeNetherWastesSmooth
			 + vec3(0.04, 0.24, 0.2) 	* BiomeWarpedForestSmooth
			 + vec3(0.3, 0.03, 0.01) 	* BiomeCrimsonForestSmooth
			 + vec3(0.012, 0.055, 0.06) * BiomeSoulSandValleySmooth
			 + vec3(0.5) 				* BiomeBasaltDeltasSmooth;
	}

	void NetherFog(inout vec3 color, in float dist) {
        vec3 transmittance = exp2(-NetherFogColor() * dist * 0.01);
        vec3 scattering = oneMinus(transmittance) * 0.6;
		//color = mix(color, GammaToLinear(fogColor), vec3(fogFactor));
		color = color * transmittance + scattering;
	}
#else
	vec4 SpatialUpscale(in sampler2D tex, in vec2 coord, in float linearDepth) {
		ivec2 bias = ivec2(coord + frameCounter) % 2;
		ivec2 texel = ivec2(coord * 0.5) + bias * 2;

		ivec2 offset[4] = ivec2[4](
			ivec2(-2,-2), ivec2(-2, 0),
			ivec2( 0, 0), ivec2( 0,-2)
		);

		float sigmaZ = 64.0 / linearDepth;

		vec4 total = vec4(0.0);
		float sumWeight = 0.0;

		for (uint i = 0u; i < 4u; ++i) {
			ivec2 sampleTexel = texel + offset[i];

			float sampleDepth = GetDepthLinear(GetDepth(sampleTexel * 2));

			float weight = max(exp2(-abs(sampleDepth - linearDepth) * sigmaZ), 1e-6);
			total += texelFetch(tex, sampleTexel, 0) * weight;

			sumWeight += weight;
		}

		return total / sumWeight;
	}

	#if defined DISTANT_HORIZONS
		vec4 SpatialUpscaleDH(in sampler2D tex, in vec2 coord, in float linearDepth) {
			ivec2 bias = ivec2(coord + frameCounter) % 2;
			ivec2 texel = ivec2(coord * 0.5) + bias * 2;

			ivec2 offset[4] = ivec2[4](
				ivec2(-2,-2), ivec2(-2, 0),
				ivec2( 0, 0), ivec2( 0,-2)
			);

			float sigmaZ = 64.0 / linearDepth;

			vec4 total = vec4(0.0);
			float sumWeight = 0.0;

			for (uint i = 0u; i < 4u; ++i) {
				ivec2 sampleTexel = texel + offset[i];

				float sampleDepth = GetDepthLinearDH(GetDepthDH(sampleTexel * 2));

				float weight = max(exp2(-abs(sampleDepth - linearDepth) * sigmaZ), 1e-6);
				total += texelFetch(tex, sampleTexel, 0) * weight;

				sumWeight += weight;
			}

			return total / sumWeight;
		}
	#endif
#endif

void TransparentAbsorption(inout vec3 color, in vec4 stainedGlassAlbedo) {
	vec3 stainedGlassColor = normalize(stainedGlassAlbedo.rgb + 1e-6) * pow(dotSelf(stainedGlassAlbedo.rgb), GLASS_TEXTURE_ALPHA * 0.25);

	color *= pow4(mix(vec3(1.0), saturate(stainedGlassColor), pow(stainedGlassAlbedo.a, 0.2 * GLASS_TEXTURE_ALPHA)));
}
