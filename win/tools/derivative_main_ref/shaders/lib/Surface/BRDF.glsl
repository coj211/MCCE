
//--// Fresnel //-------------------------------------------------------------//

// 空气——电介质
float FresnelSchlick(in float cosTheta, in float f0) {
    float f = pow5(1.0 - cosTheta);
    return saturate(f + oneMinus(f) * f0);
}

// 电介质——电介质
float FresnelDielectric(in float cosTheta, in float f0) { // 基于反射率f0
    f0 = min(sqrt(f0), 0.99999);
    f0 = (1.0 + f0) * rcp(1.0 - f0);

    float cosR = 1.0 - sqr(sqrt(1.0 - sqr(cosTheta)) * rcp(max(f0, 1e-16)));
    if (cosR < 0.0) return 1.0;

    cosR = sqrt(cosR);
    float a = f0 * cosTheta;
    float b = f0 * cosR;
    float r1 = (a - cosR) / (a + cosR);
    float r2 = (b - cosTheta) / (b + cosTheta);
    return saturate(0.5 * (r1 * r1 + r2 * r2));
}

float FresnelDielectricN(in float cosTheta, in float n) { // 基于折射系数ior
    float cosR = sqr(n) + sqr(cosTheta) - 1.0;
    if (cosR < 0.0) return 1.0;

    cosR = sqrt(cosR);
    float a = n * cosTheta;
    float b = n * cosR;
    float r1 = (a - cosR) / (a + cosR);
    float r2 = (b - cosTheta) / (b + cosTheta);
    return saturate(0.5 * (r1 * r1 + r2 * r2));
}

//--// SmithGGX //------------------------------------------------------------//

float V1SmithGGXInverse(in float cosTheta, in float alpha2) {
    return cosTheta + sqrt((cosTheta - alpha2 * cosTheta) * cosTheta + alpha2);
}

float V2SmithGGX(in float NdotV, in float NdotL, in float alpha2) {
    float ggxl = NdotL * sqrt(alpha2 + (NdotV - NdotV * alpha2) * NdotV);
    float ggxv = NdotV * sqrt(alpha2 + (NdotL - NdotL * alpha2) * NdotL);
    return 0.5 / (ggxl + ggxv);
}

float DistributionGGX(in float NdotH, in float alpha2) {
	return alpha2 * rPI / sqr(1.0 + (NdotH * alpha2 - NdotH) * NdotH);
}

vec3 DiffuseHammon(in float LdotV, in float NdotV, in float NdotL, in float NdotH, in float roughness, in vec3 albedo) {
	if (NdotL < 1e-6) return vec3(0.0);
    float facing = max0(LdotV) * 0.5 + 0.5;

    //float singleSmooth = rcp(1.0 - (4.0 * sqrt(f0) + 5.0 * f0 * f0)) * 9.0 * fresnelSchlickInverse(f0, NdotL) * fresnelSchlickInverse(f0, NdotV);
    //float singleSmooth = 1.05 * FresnelSchlickInverse(NdotL, 0.0) * FresnelSchlickInverse(NdotV, 0.0);
    float singleSmooth = 1.05 * oneMinus(pow5(1.0 - max(NdotL, 1e-2))) * oneMinus(pow5(1.0 - max(NdotV, 1e-2)));
    float singleRough = facing * (0.45 - 0.2 * facing) * (rcp(NdotH) + 2.0);

    float single = mix(singleSmooth, singleRough, roughness) * rPI;
    float multi = 0.1159 * roughness;

    return (multi * albedo + single) * NdotL;
}

float SpecularBRDF(in float LdotH, in float NdotV, in float NdotL, in float NdotH, in float alpha2, in float f0) {
	if (NdotL < 1e-5) return 0.0;
    float F = FresnelSchlick(LdotH, f0);
	//if (F < 1e-2) return 0.0;

	float D = DistributionGGX(NdotH, alpha2);
    float V = V2SmithGGX(max(NdotV, 1e-2), max(NdotL, 1e-2), alpha2);

	return min(NdotL * D * V * F, 4.0);
}
