
out vec3 albedoData;

/* DRAWBUFFERS:6 */

uniform sampler2D tex;

in vec2 texcoord;

void main() {
	albedoData = texture(tex, texcoord).rgb;
}
