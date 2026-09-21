
#include "/lib/Surface/BRDF.glsl"

#define RAYTRACE_SAMPLES 16 // [4 8 12 16 24 32 48 64 128 256 512]
//#define REAL_SKY_REFLECTION

#define RAYTRACE_REFINEMENT // Improves ray trace quality by refining the rays with minimal performance overhead.
#define RAYTRACE_REFINEMENT_STEPS 6 // [2 3 4 5 6 7 8 9 10 12 14 16 18 20 22 24 26 28 30 32]

bool ScreenSpaceRayTrace(in vec3 viewPos, in vec3 viewDir, in float dither, in uint steps, inout vec3 rayPos) {
    const float maxLength = 1.0 / steps;
    const float minLength = length(screenPixelSize);

    vec3 position = ViewToScreenSpace(viewDir * abs(viewPos.z) + viewPos);
    vec3 screenDir = normalize(position - rayPos);
    float stepWeight = 1.0 / abs(screenDir.z);

    float stepLength = minOf((step(0.0, screenDir) - rayPos) / screenDir) * rcp(steps);

    screenDir.xy *= screenSize;
    rayPos.xy *= screenSize;

    vec3 rayStep = screenDir * stepLength;
    rayPos += rayStep * dither + screenDir * minLength;

    #ifdef REAL_SKY_REFLECTION
        bool hitSky = false;
    #endif
	bool hit = false;

    float depth = texelFetch(depthtex1, ivec2(rayPos.xy), 0).x;

    for (uint i = 0u; i < steps; ++i) {
        if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) return false;
        // if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) break;
        if (rayPos.z >= 1.0) {
            #ifdef REAL_SKY_REFLECTION
                hitSky = true;
            #endif
            break;
        }

        stepLength = abs(depth - rayPos.z) * stepWeight;
        rayPos += screenDir * clamp(stepLength, minLength, maxLength);

        depth = texelFetch(depthtex1, ivec2(rayPos.xy), 0).x;

        if (depth < rayPos.z) {
            float linearSample = GetDepthLinear(depth);
            float currentDepth = GetDepthLinear(rayPos.z);
            if (abs(linearSample - currentDepth) / currentDepth < 0.2) {
                hit = true;
                break;
            }
        }
    }

	if (!hit) return false;

    rayStep = screenDir * stepLength;

    #ifdef RAYTRACE_REFINEMENT
        for (uint i = 0u; i < RAYTRACE_REFINEMENT_STEPS; ++i) {
            if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) break;

            rayStep *= 0.5;

            depth = texelFetch(depthtex1, ivec2(rayPos.xy), 0).x;

            if (depth < rayPos.z) {
                rayPos -= rayStep;
            } else {
                rayPos += rayStep;
            }
        }
    #endif

    #ifdef REAL_SKY_REFLECTION
        return depth - isEyeInWater >= 1.0 && hitSky; // Real sky reflection
    #else
        return depth < 1.0;
    #endif
}

#if defined DISTANT_HORIZONS
    bool ScreenSpaceRayTraceDH(in vec3 viewPos, in vec3 viewDir, in float dither, in uint steps, inout vec3 rayPos) {
        const float maxLength = 1.0 / steps;
        const float minLength = length(screenPixelSize);

        vec3 position = ViewToScreenSpaceDH(viewDir * abs(viewPos.z) + viewPos);
        vec3 screenDir = normalize(position - rayPos);
        float stepWeight = 1.0 / abs(screenDir.z);

        float stepLength = minOf((step(0.0, screenDir) - rayPos) / screenDir) * rcp(steps);

        screenDir.xy *= screenSize;
        rayPos.xy *= screenSize;

        vec3 rayStep = screenDir * stepLength;
        rayPos += rayStep * dither + screenDir * minLength;

        #ifdef REAL_SKY_REFLECTION
            bool hitSky = false;
        #endif
        bool hit = false;

        float depth = texelFetch(dhDepthTex1, ivec2(rayPos.xy), 0).x;

        for (uint i = 0u; i < steps; ++i) {
            if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) return false;
            // if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) break;
            if (rayPos.z >= 1.0) {
                #ifdef REAL_SKY_REFLECTION
                    hitSky = true;
                #endif
                break;
            }

            stepLength = abs(depth - rayPos.z) * stepWeight;
            rayPos += screenDir * clamp(stepLength, minLength, maxLength);

            depth = texelFetch(dhDepthTex1, ivec2(rayPos.xy), 0).x;

            if (depth < rayPos.z) {
                float linearSample = GetDepthLinear(depth);
                float currentDepth = GetDepthLinear(rayPos.z);
                if (abs(linearSample - currentDepth) / currentDepth < 0.2) {
                    hit = true;
                    break;
                }
            }
        }

        if (!hit) return false;

        rayStep = screenDir * stepLength;

        #ifdef RAYTRACE_REFINEMENT
            for (uint i = 0u; i < RAYTRACE_REFINEMENT_STEPS; ++i) {
                if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) break;

                rayStep *= 0.5;

                depth = texelFetch(dhDepthTex1, ivec2(rayPos.xy), 0).x;

                if (depth < rayPos.z) {
                    rayPos -= rayStep;
                } else {
                    rayPos += rayStep;
                }
            }
        #endif

        #ifdef REAL_SKY_REFLECTION
            return depth - isEyeInWater >= 1.0 && hitSky; // Real sky reflection
        #else
            return depth < 1.0;
        #endif
    }
#endif

// https://ggx-research.github.io/publication/2023/06/09/publication-ggx.html
vec3 sampleGGXVNDF(in vec3 viewDir, in float roughness, in vec2 xy) {
    #define SPECULAR_TAIL_CLAMP

    #ifdef SPECULAR_TAIL_CLAMP
        xy.y = clamp(xy.y * 0.25, 1e-3, 0.25);
    #endif
    // Transform viewer direction to the hemisphere configuration
    viewDir = normalize(vec3(roughness * viewDir.xy, viewDir.z));

    // Sample a reflection direction off the hemisphere
    float phi = TAU * xy.x;
    float cosTheta = oneMinus(xy.y) * (1.0 + viewDir.z) - viewDir.z;
    float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
    vec3 reflected = vec3(cossin(phi) * sinTheta, cosTheta);

    // Evaluate halfway direction
    // This gives the normal on the hemisphere
    vec3 halfway = reflected + viewDir;

    // Transform the halfway direction back to hemiellispoid configuation
    // This gives the final sampled normal
    return normalize(vec3(roughness * halfway.xy, halfway.z));
}
