
flat out float exposure;

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

#ifdef DOF_ENABLED
    flat out float centerDepthSmooth;
#endif

//----// FUNCTIONS //-----------------------------------------------------------------------------//

float CalculateAverageExposure() {
    const float tileSize = exp2(float(AUTO_EXPOSURE_LOD));

	ivec2 tileSteps = ivec2(screenSize * rcp(tileSize));

    float exposure = 0.0;
    float sumWeight = 0.0;

	for (uint x = 0u; x < tileSteps.x; ++x) {
        for (uint y = 0u; y < tileSteps.y; ++y) {
            float luminance = GetLuminance(texelFetch(colortex4, ivec2(x, y), AUTO_EXPOSURE_LOD).rgb);

            float weight = 1.0 - remap(0.25, 0.75, length(vec2(x, y) / tileSteps * 2.0 - 1.0));
            weight = curve(weight) * 0.9 + 0.1;

            exposure += max(log(luminance), -18.0) * weight;
            sumWeight += weight;
        }
	}

    exposure /= max(sumWeight, 1.0);

	return fastExp(exposure * 0.75);
}

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	gl_Position = vec4(gl_Vertex.xy * 2.0 - 1.0, 0.0, 1.0);

 	#ifdef AUTO_EXPOSURE
		exposure = CalculateAverageExposure();

        #if defined IS_END
            const float K = 12.0;
        #else
            const float K = 19.0;
        #endif

        const float calibration = exp2(AUTO_EXPOSURE_BIAS) * K * 1e-2;

        const float a = K * 1e-2 * 18.0;
        const float b = a - K * 1e-2 * 0.04;

        float targetExposure = calibration / (a - b * fastExp(-exposure * rcp(b)));

        float prevExposure = clamp16F(texelFetch(colortex5, ivec2(0), 0).a);

        float speed = targetExposure < prevExposure ? 1.5 : 1.0;
        exposure = mix(targetExposure, prevExposure, fastExp(-speed * frameTime * EXPOSURE_SPEED));
	#else
		exposure = rcp(MANUAL_EXPOSURE_VALUE) * 0.8;
	#endif

    #ifdef DOF_ENABLED
        float centerDepth = texelFetch(depthtex2, ivec2(screenSize * 0.5), 0).x * 2.0 - 1.0;
        centerDepth = 1.0 / (centerDepth * gbufferProjectionInverse[2][3] + gbufferProjectionInverse[3][3]);
        float prevCenterDepth = texelFetch(colortex5, ivec2(1), 0).a;
        centerDepthSmooth = mix(prevCenterDepth, centerDepth, saturate(fastExp(-0.1 / (frameTime * FOCUSING_SPEED)) / (centerDepth + 0.2)));
    #endif
}
