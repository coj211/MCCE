
//#define RAYTRACED_REFRACTION
//#define REFRACTIVE_DISPERSION

vec3 fastRefract(in vec3 dir, in vec3 normal, in float eta) {
    float NdotD = dot(normal, dir);
    float k = 1.0 - eta * eta * oneMinus(NdotD * NdotD);
    if (k < 0.0) return vec3(0.0);

    return dir * eta - normal * (sqrt(k) + NdotD * eta);
}

#ifdef RAYTRACED_REFRACTION

#define RAYTRACE_SAMPLES 16 // [4 8 12 16 24 32 48 64 128 256 512]

bool ScreenSpaceRayTrace(in vec3 viewPos, in vec3 viewDir, in float dither, in uint steps, inout vec3 rayPos) {
    const float maxLength = 1.0 / steps;
    const float minLength = length(screenPixelSize);
    //float maxDist = far * sqrt(3.0);
    //float rayLength = ((viewPos.z + rayDir.z * maxDist) > -near) ?
    //                (-near - viewPos.z) / rayDir.z : maxDist;

    //vec3 position = ViewToScreenSpace(rayDir * rayLength + viewPos);
    vec3 position = ViewToScreenSpace(viewDir * abs(viewPos.z) + viewPos);
    vec3 screenDir = normalize(position - rayPos);
    float stepWeight = 1.0 / abs(screenDir.z);

    float stepLength = minOf((step(0.0, screenDir) - rayPos) / screenDir) * rcp(steps);

    screenDir.xy *= screenSize;
    rayPos.xy *= screenSize;

    vec3 rayStep = screenDir * stepLength;
    rayPos += rayStep * dither + screenDir * minLength;

    float depthTolerance = max(abs(rayStep.z) * 3.0, 0.02 / sqr(viewPos.z)); // From DrDesten

    for (uint i = 0u; i < steps; ++i) {
        // if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) return false;
        if (clamp(rayPos.xy, vec2(0.0), screenSize) != rayPos.xy) break;
        if (rayPos.z >= 1.0) break;

        float depth = texelFetch(depthtex1, ivec2(rayPos.xy), 0).x;
        stepLength = abs(depth - rayPos.z) * stepWeight;
        rayPos += screenDir * clamp(stepLength, minLength, maxLength);

        //if (depth < rayPos.z) {
        //    float linearSample = ScreenToViewSpace(depth);
        //    float currentDepth = ScreenToViewSpace(rayPos.z);
        //    if (abs(linearSample - currentDepth) / currentDepth < 0.2) {
        if (depth < rayPos.z && abs(depthTolerance - (rayPos.z - depth)) < depthTolerance) {
            return true;
            // break;
        }
    }

    return false;
}

vec2 CalculateRefractCoord(in TranslucentMask mask, in vec3 normal, in vec3 viewDir, in vec3 viewPos, in float depth, in float ior) {
	if (!mask.translucent) return screenCoord;

	vec3 refractedDir = fastRefract(viewDir, normal, 1.0 / ior);

    vec3 hitPos = vec3(screenCoord, depth);
	if (ScreenSpaceRayTrace(viewPos, refractedDir, InterleavedGradientNoiseTemporal(gl_FragCoord.xy), RAYTRACE_SAMPLES, hitPos)) {
		hitPos.xy *= screenPixelSize;
	} else {
		hitPos.xy = ViewToScreenSpace(viewPos + refractedDir * 0.5).xy;
	}

	return saturate(hitPos.xy);
}

#else

#include "/lib/Water/WaterWave.glsl"

vec2 CalculateRefractCoord(in TranslucentMask mask, in vec3 normal, in vec3 worldPos, in vec3 viewPos, in float depth, in float depthT) {
	if (!mask.translucent) return screenCoord;

	vec2 refractCoord;
	float waterDepth = GetDepthLinear(depthT);
	float refractionDepth = GetDepthLinear(depth) - waterDepth;

	if (mask.water) {
        worldPos += cameraPosition;
		vec3 wavesNormal = GetWavesNormal(worldPos.xz - worldPos.y).xzy;
		vec3 waterNormal = mat3(gbufferModelView) * wavesNormal;
		vec3 wavesNormalView = normalize(waterNormal);

		vec3 nv = normalize(gbufferModelView[1].xyz);

		refractCoord = nv.xy - wavesNormalView.xy;
		refractCoord *= saturate(refractionDepth) * 0.5 / (waterDepth + 1e-4);
		refractCoord += screenCoord;
	} else {
		vec3 refractDir = fastRefract(normalize(viewPos), normal, 1.0 / GLASS_REFRACT_IOR);
		refractDir /= saturate(dot(refractDir, -normal));
		refractDir *= saturate(refractionDepth * 2.0) * 0.25;

		refractCoord = ViewToScreenSpace(viewPos + refractDir).xy;
	}

	//float currentDepth = texture(depthtex0, screenCoord).x;
	float refractDepth = texture(depthtex1, refractCoord).x;
	if (refractDepth < depthT) return screenCoord;

	return saturate(refractCoord);
}

#if defined DISTANT_HORIZONS
	vec2 CalculateRefractCoordDH(in TranslucentMask mask, in vec3 normal, in vec3 worldPos, in vec3 viewPos, in float depth, in float depthT) {
		if (!mask.translucent) return screenCoord;

		vec2 refractCoord;
		float waterDepth = GetDepthLinearDH(depthT);
		float refractionDepth = GetDepthLinearDH(depth) - waterDepth;

		if (mask.water) {
			worldPos += cameraPosition;
			vec3 wavesNormal = GetWavesNormal(worldPos.xz - worldPos.y).xzy;
			vec3 waterNormal = mat3(gbufferModelView) * wavesNormal;
			vec3 wavesNormalView = normalize(waterNormal);

			vec3 nv = normalize(gbufferModelView[1].xyz);

			refractCoord = nv.xy - wavesNormalView.xy;
			refractCoord *= saturate(refractionDepth) * 0.5 / (waterDepth + 1e-4);
			refractCoord += screenCoord;
		} else {
			vec3 refractDir = fastRefract(normalize(viewPos), normal, 1.0 / GLASS_REFRACT_IOR);
			refractDir /= saturate(dot(refractDir, -normal));
			refractDir *= saturate(refractionDepth * 2.0) * 0.25;

			refractCoord = ViewToScreenSpaceDH(viewPos + refractDir).xy;
		}

		//float currentDepth = texture(depthtex0, screenCoord).x;
		float refractDepth = texture(dhDepthTex1, refractCoord).x;
		if (refractDepth < depthT) return screenCoord;

		return saturate(refractCoord);
	}
#endif

#endif