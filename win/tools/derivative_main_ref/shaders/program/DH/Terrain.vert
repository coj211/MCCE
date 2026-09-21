
uniform vec2 taaOffset;
uniform mat4 gbufferModelViewInverse;
uniform mat4 dhProjection;

out vec3 tint;

out vec3 flatNormal;
out vec3 worldPos;

flat out uint materialIDs;

out vec2 lightmap;

#include "/lib/Head/Common.inc"

void main() {
	tint = gl_Color.rgb;

	lightmap = mat2(gl_TextureMatrix[1]) * gl_MultiTexCoord1.xy + gl_TextureMatrix[1][3].xy;
	lightmap = saturate((lightmap - 0.03125) * 1.06667);

	flatNormal = normalize(gl_NormalMatrix * gl_Normal);

	vec4 viewPos = gl_ModelViewMatrix * gl_Vertex;
	gl_Position = dhProjection * viewPos;

	worldPos = transMAD(gbufferModelViewInverse, viewPos.xyz);

	#ifdef TAA_ENABLED
		gl_Position.xy += taaOffset * gl_Position.w;
	#endif

	materialIDs = 0u;
	switch(dhMaterialId){
        case DH_BLOCK_LEAVES: case DH_BLOCK_SNOW:
            materialIDs = 7u;
            break;
        case DH_BLOCK_LAVA:
            materialIDs = 15u;
            break;
        case DH_BLOCK_ILLUMINATED:
            materialIDs = 20u;
            break;
    }
}