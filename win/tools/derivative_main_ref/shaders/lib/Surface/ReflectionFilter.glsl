
vec4 ReflectionFilter(in ivec2 texel, in vec4 reflectionData, in float roughness, in vec3 normal, in vec3 viewDir, in float size, in vec2 dither) {
    float smoothness = 1.0 - sqrt(roughness);
    float linearDepth = GetDepthLinear(texel);
    float NdotV = saturate(dot(-viewDir, normal));

    float coordOffset = 8.0 * size * min(roughness * 20.0, 1.0) * oneMinus(fastExp(-sqrt(reflectionData.a) * 50.0));
    coordOffset *= reflectionData.w * 0.8 + 0.2;

    float sharpenWeight = reflectionData.w * 0.475 + 0.025;
    float roughnessInv = 1e2 / max(roughness, 1e-5);

    reflectionData.rgb = pow(dotSelf(reflectionData.rgb), 0.5 * sharpenWeight) * normalize(max(reflectionData.rgb, 1e-6));
    float sumWeight = 1.0;

    for (uint i = 0u; i < 8u; ++i) {
        ivec2 sampleTexel = clamp(texel + ivec2((offset3x3N[i] + dither) * coordOffset), ivec2(0), ivec2(screenSize - 1));

        vec4 sampleData = texelFetch(colortex2, sampleTexel, 0);

        float sampleLinerDepth = GetDepthLinear(sampleTexel);

        float weight = pow(max(dot(normal, GetNormals(sampleTexel)), 1e-6), roughnessInv) *
                fastExp(-abs(reflectionData.w - sampleData.w) * smoothness) *
                fastExp(-abs(sampleLinerDepth - linearDepth) * 2.0 * NdotV * inversesqrt(coordOffset));

        reflectionData += vec4(pow(dotSelf(sampleData.rgb), 0.5 * sharpenWeight) * normalize(max(sampleData.rgb, 1e-6)), sampleData.a) * weight;
        sumWeight += weight;
    }
    if (sumWeight < 1e-3) return reflectionData;

    reflectionData /= sumWeight;
    reflectionData.rgb = pow(dotSelf(reflectionData.rgb), 0.5 / sharpenWeight) * normalize(max(reflectionData.rgb, 1e-6));

    return reflectionData;
}
