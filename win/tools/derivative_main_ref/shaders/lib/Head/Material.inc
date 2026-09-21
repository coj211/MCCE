
struct Material {
	//float smoothness;
	float roughness;
	float isMetal;
	float f0;
	float emissiveness;
	bool hasReflections;
	bool isRough;
};

#define EMISSIVE_CURVE 2.2 // [1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.2 2.3 2.4 2.5 2.6 2.7 2.8 2.9 3.0]

Material GetMaterialData(in vec4 specTex) {
	Material material;

	//material.smoothness = specTex.r;
	material.roughness = sqr(1.0 - specTex.r);
	#if defined MC_SPECULAR_MAP
		#if TEXTURE_FORMAT == 0
			if (specTex.g > (229.5 / 255.0)) {
				material.isMetal = 1.0;
				material.f0 = 0.91;
			} else {
				material.isMetal = 0.0;
				material.f0 = specTex.g;			
			}
			material.emissiveness = specTex.a == 1.0 ? 0.0 : specTex.a;
		#else
			material.isMetal = specTex.g;
			material.f0 = specTex.g * 0.96 + 0.04;
			material.emissiveness = specTex.b;
		#endif

		material.emissiveness = pow(material.emissiveness, EMISSIVE_CURVE);
	#else
		material.isMetal = 0.0;
		material.f0 = specTex.g;
		material.emissiveness = 0.0;
	#endif

	material.hasReflections = specTex.r + material.isMetal > 5e-3;
	material.isRough = material.roughness > ROUGH_REFLECTIONS_THRESHOLD;

	return material;
}

Material GetMaterialData(in vec2 specTex) {
	Material material;

	material.roughness = specTex.r;
	#if defined MC_SPECULAR_MAP
		#if TEXTURE_FORMAT == 0
			if (specTex.g > (229.5 / 255.0)) {
				material.isMetal = 1.0;
				material.f0 = 0.91;
			} else {
				material.isMetal = 0.0;
				material.f0 = specTex.g;			
			}
		#else
			material.isMetal = specTex.g;
			material.f0 = specTex.g * 0.96 + 0.04;
		#endif
	#else
		material.isMetal = 0.0;
		material.f0 = specTex.g;
	#endif

	material.hasReflections = max0(0.625 - material.roughness) + material.isMetal > 5e-3;
	material.isRough = material.roughness > ROUGH_REFLECTIONS_THRESHOLD;

	return material;
}
