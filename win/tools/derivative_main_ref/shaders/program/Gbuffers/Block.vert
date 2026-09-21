
uniform vec2 taaOffset;
uniform int blockEntityId;
uniform vec3 cameraPosition;
uniform mat4 gbufferModelViewInverse;

//#if MC_VERSION < 11500
//	layout(location = 12) in vec4 at_tangent;
//#else
//	layout(location = 13) in vec4 at_tangent;
//#endif

out vec4 tint;
out vec2 texcoord;
out vec3 minecraftPos;
out vec4 viewPos;

out vec2 lightmap;

//flat out mat3 tbnMatrix;

flat out int materialIDs;

#include "/Settings.glsl"

void main() {
	texcoord = gl_MultiTexCoord0.xy;

	lightmap = clamp(gl_MultiTexCoord1.xy * (1.0 / 240.0), 0.0, 1.0);

    //tbnMatrix[2] = normalize(gl_NormalMatrix * gl_Normal);
	//#if defined MC_NORMAL_MAP || defined RAIN_SPLASH_EFFECT
	//	tbnMatrix[0] = normalize(gl_NormalMatrix * at_tangent.xyz);
	//	tbnMatrix[1] = cross(tbnMatrix[0], tbnMatrix[2]) * sign(at_tangent.w);
	//#endif

	viewPos = gl_ModelViewMatrix * gl_Vertex;
	minecraftPos = mat3(gbufferModelViewInverse) * viewPos.xyz + gbufferModelViewInverse[3].xyz + cameraPosition;

	gl_Position = gl_ProjectionMatrix * viewPos;

	#ifdef TAA_ENABLED
		gl_Position.xy += taaOffset * gl_Position.w;
	#endif

	tint = gl_Color;

	materialIDs = max(blockEntityId - 10000, 0);
}