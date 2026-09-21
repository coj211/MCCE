
vec3 MoonFlux = vec3(abs(moonPhase - 4.0) * 0.25 + 0.2) * (NIGHT_BRIGHTNESS + nightVision * 0.02);

vec3 lightningColor = isLightningFlashing * vec3(0.45, 0.43, 1.0) * 0.03;

// const float planetRadius = 6371e3;

#ifdef AURORA
	float auroraAmount = smoothstep(0.0, 0.2, -worldSunVector.y) * AURORA_STRENGTH;
#endif

vec4 ToSH(float value, vec3 dir) {
	const vec2 foo = vec2(0.5 * PI * sqrt(rPI), 0.3849 * PI * sqrt(0.75 * rPI));
    return vec4(foo.x, foo.y * dir.yzx) * value;
}

vec3 FromSH(vec4 cR, vec4 cG, vec4 cB, vec3 lightDir) {
	const vec2 foo = vec2(0.5 * sqrt(rPI), sqrt(0.75 * rPI));
    vec4 sh = vec4(foo.x, foo.y * lightDir.yzx);

    return vec3(dot(sh, cR), dot(sh, cG), dot(sh, cB));
}

float RayleighPhase(in float cosTheta) {
	const float c = 3.0 / 16.0 * rPI;
	return cosTheta * cosTheta * c + c;
}

float HenyeyGreensteinPhase(in float cosTheta, in const float g) {
	const float gg = g * g;
    float phase = 1.0 + gg - 2.0 * g * cosTheta;
    return oneMinus(gg) / (4.0 * PI * phase * sqrt(phase));
}

float CornetteShanksPhase(in float cosTheta, in const float g) {
	const float gg = g * g;
  	float a = oneMinus(gg) * rcp(2.0 + gg) * 3.0 * rPI;
  	float b = (1.0 + sqr(cosTheta)) * pow((1.0 + gg - 2.0 * g * cosTheta), -1.5);
  	return a * b * 0.125;
}

float MiePhaseClouds(in float cosTheta, in const vec3 g, in const vec3 w) {
	const vec3 gg = g * g;
	vec3 a = (0.75 * oneMinus(gg)) * rcp(2.0 + gg)/* * rTAU*/;
	vec3 b = (1.0 + sqr(cosTheta)) * pow(1.0 + gg - 2.0 * g * cosTheta, vec3(-1.5));

	return dot(a * b, w) / (w.x + w.y + w.z);
}

vec3 DoNightEye(in vec3 color) {
	float luminance = GetLuminance(color);
	float rodFactor = exp2(-luminance * 6e2);
	return mix(color, luminance * vec3(0.72, 0.95, 1.2), rodFactor);
}

float fastAcos(in float x) {
    float a = abs(x);
	float r = 1.570796 - 0.175394 * a;
	r *= sqrt(1.0 - a);

	return x < 0.0 ? PI - r : r;
}

vec2 ProjectSky(in vec3 direction) {
	vec2 coord = vec2(atan(-direction.x, -direction.z) * rTAU + 0.5, fastAcos(direction.y) * rPI);

	coord.x = coord.x * oneMinus(4.0 / skyCaptureRes.x) + 2.0 / skyCaptureRes.x;

	// coord.x *= 255.0 / 256.0;
	return saturate(coord * skyCaptureRes * screenPixelSize);
}

vec3 UnprojectSky(in vec2 coord) {
	coord.x *= 256.0 / 255.0;
	coord.x = fract((coord.x - 2.0 / skyCaptureRes.x) * rcp(oneMinus(4.0 / skyCaptureRes.x)));

	coord *= vec2(TAU, PI);

	return vec3(sincos(coord.x) * sin(coord.y), cos(coord.y)).xzy;
}

vec2 RaySphereIntersection(in vec3 pos, in vec3 dir, in float rad) {
	float PdotD = dot(pos, dir);
	float delta = sqr(PdotD) + sqr(rad) - dotSelf(pos);

	if (delta < 0.0) return vec2(-1.0);

	delta = sqrt(delta);

	return vec2(-delta, delta) - PdotD;
}

//----------------------------------------------------------------------------//

const float planetRadius = 6371e3;

// const float sun_angular_radius = 0.004675;
const float sun_angular_radius = 0.012; // Unphysical
const float mie_phase_g = 0.77;

#if defined PRECOMPUTED_ATMOSPHERIC_SCATTERING

//#define SKY_GROUND
//#define FULL_AERIAL_PERSPECTIVE


#define TRANSMITTANCE_TEXTURE_WIDTH     256.0
#define TRANSMITTANCE_TEXTURE_HEIGHT    64.0

#define SCATTERING_TEXTURE_R_SIZE       32.0
#define SCATTERING_TEXTURE_MU_SIZE      128.0
#define SCATTERING_TEXTURE_MU_S_SIZE    32.0
#define SCATTERING_TEXTURE_NU_SIZE      8.0

#define IRRADIANCE_TEXTURE_WIDTH        64.0
#define IRRADIANCE_TEXTURE_HEIGHT       16.0

// https://ebruneton.github.io/precomputed_atmospheric_scattering/atmosphere/functions.glsl.html

struct AtmosphereParameters {
    // The solar irradiance at the top of the atmosphere.
    vec3 solar_irradiance;
    // The sun's angular radius. Warning: the implementation uses approximations
    // that are valid only if this angle is smaller than 0.1 radians.
//    float sun_angular_radius;
    // The distance between the planet center and the bottom of the atmosphere.
//    float bottom_radius;
    // The distance between the planet center and the top of the atmosphere.
//    float top_radius;
    // The density profile of air molecules, i.e. a function from altitude to
    // dimensionless values between 0 (null density) and 1 (maximum density).
//    DensityProfile rayleigh_density;
    // The scattering coefficient of air molecules at the altitude where their
    // density is maximum (usually the bottom of the atmosphere), as a function of
    // wavelength. The scattering coefficient at altitude h is equal to
    // 'rayleigh_scattering' times 'rayleigh_density' at this altitude.
    vec3 rayleigh_scattering;
    // The density profile of aerosols, i.e. a function from altitude to
    // dimensionless values between 0 (null density) and 1 (maximum density).
//    DensityProfile mie_density;
    // The scattering coefficient of aerosols at the altitude where their density
    // is maximum (usually the bottom of the atmosphere), as a function of
    // wavelength. The scattering coefficient at altitude h is equal to
    // 'mie_scattering' times 'mie_density' at this altitude.
    vec3 mie_scattering;
    // The extinction coefficient of aerosols at the altitude where their density
    // is maximum (usually the bottom of the atmosphere), as a function of
    // wavelength. The extinction coefficient at altitude h is equal to
    // 'mie_extinction' times 'mie_density' at this altitude.
//    vec3 mie_extinction;
    // The asymetry parameter for the Cornette-Shanks phase function for the
    // aerosols.
//    float mie_phase_function_g;
    // The density profile of air molecules that absorb light (e.g. ozone), i.e.
    // a function from altitude to dimensionless values between 0 (null density)
    // and 1 (maximum density).
//    DensityProfile absorption_density;
    // The extinction coefficient of molecules that absorb light (e.g. ozone) at
    // the altitude where their density is maximum, as a function of wavelength.
    // The extinction coefficient at altitude h is equal to
    // 'absorption_extinction' times 'absorption_density' at this altitude.
//    vec3 absorption_extinction;
    // The average albedo of the ground.
    vec3 ground_albedo;
    // The cosine of the maximum Sun zenith angle for which atmospheric scattering
    // must be precomputed (for maximum precision, use the smallest Sun zenith
    // angle yielding negligible sky light radiance values. For instance, for the
    // Earth case, 102 degrees is a good choice - yielding mu_s_min = -0.2).
//    float mu_s_min;
};

AtmosphereParameters atmosphereModel = AtmosphereParameters(
    // The solar irradiance at the top of the atmosphere.
    vec3(1.474000,1.850400,1.911980),
    // The sun's angular radius. Warning: the implementation uses approximations
    // that are valid only if this angle is smaller than 0.1 radians.
//    0.004675,
    // The distance between the planet center and the bottom of the atmosphere.
//    6360.000000,
    // The distance between the planet center and the top of the atmosphere.
//    6420.000000,
    // The density profile of air molecules, i.e. a function from altitude to
    // dimensionless values between 0 (null density) and 1 (maximum density).
//    DensityProfile(DensityProfileLayer[2](DensityProfileLayer(0.000000,0.000000,0.000000,0.000000,0.000000),DensityProfileLayer(0.000000,1.000000,-0.125000,0.000000,0.000000))),
    // The scattering coefficient of air molecules at the altitude where their
    // density is maximum (usually the bottom of the atmosphere), as a function of
    // wavelength. The scattering coefficient at altitude h is equal to
    // 'rayleigh_scattering' times 'rayleigh_density' at this altitude.
    vec3(0.005802, 0.013558, 0.033100),
    // The density profile of aerosols, i.e. a function from altitude to
    // dimensionless values between 0 (null density) and 1 (maximum density).
//    DensityProfile(DensityProfileLayer[2](DensityProfileLayer(0.000000,0.000000,0.000000,0.000000,0.000000),DensityProfileLayer(0.000000,1.000000,-0.833333,0.000000,0.000000))),
    // The scattering coefficient of aerosols at the altitude where their density
    // is maximum (usually the bottom of the atmosphere), as a function of
    // wavelength. The scattering coefficient at altitude h is equal to
    // 'mie_scattering' times 'mie_density' at this altitude.
    vec3(0.003996, 0.003996, 0.003996),
    // The extinction coefficient of aerosols at the altitude where their density
    // is maximum (usually the bottom of the atmosphere), as a function of
    // wavelength. The extinction coefficient at altitude h is equal to
    // 'mie_extinction' times 'mie_density' at this altitude.
//    vec3(0.004440, 0.004440, 0.004440),
    // The asymetry parameter for the Cornette-Shanks phase function for the
    // aerosols.
//    0.800000,
    // The density profile of air molecules that absorb light (e.g. ozone), i.e.
    // a function from altitude to dimensionless values between 0 (null density)
    // and 1 (maximum density).
//    DensityProfile(DensityProfileLayer[2](DensityProfileLayer(25.000000,0.000000,0.000000,0.066667,-0.666667),DensityProfileLayer(0.000000,0.000000,0.000000,-0.066667,2.666667))),
    // The extinction coefficient of molecules that absorb light (e.g. ozone) at
    // the altitude where their density is maximum, as a function of wavelength.
    // The extinction coefficient at altitude h is equal to
    // 'absorption_extinction' times 'absorption_density' at this altitude.
//    vec3(0.000650, 0.001881, 0.000085),
    // The average albedo of the ground.
    vec3(0.1)//,
    // The cosine of the maximum Sun zenith angle for which atmospheric scattering
    // must be precomputed (for maximum precision, use the smallest Sun zenith
    // angle yielding negligible sky light radiance values. For instance, for the
    // Earth case, 102 degrees is a good choice - yielding mu_s_min = -0.2).
//    -0.500000
);

#define ATMOSPHERE_BOTTOM_ALTITUDE  1000.0 // [0.0 500.0 1000.0 2000.0 3000.0 4000.0 5000.0 6000.0 7000.0 8000.0 9000.0 10000.0 11000.0 12000.0 13000.0 14000.0 15000.0 16000.0]
#define ATMOSPHERE_TOP_ALTITUDE     100000.0 // [0.0 5000.0 10000.0 20000.0 30000.0 40000.0 50000.0 60000.0 70000.0 80000.0 90000.0 100000.0 110000.0 120000.0 130000.0 140000.0 150000.0 160000.0]

const float atmosphere_bottom_radius = planetRadius - ATMOSPHERE_BOTTOM_ALTITUDE;
const float atmosphere_top_radius = planetRadius + ATMOSPHERE_TOP_ALTITUDE;

const float atmosphere_bottom_radius_sq = atmosphere_bottom_radius * atmosphere_bottom_radius;
const float atmosphere_top_radius_sq = atmosphere_top_radius * atmosphere_top_radius;

const float mu_s_min = -0.2;

//--// Utility functions //---------------------------------------------------//

float ClampCosine(float mu) {
    return clamp(mu, -1.0, 1.0);
}

float ClampRadius(/*AtmosphereParameters atmosphere, */float r) {
    return clamp(r, atmosphere_bottom_radius, atmosphere_top_radius);
}

float SafeSqrt(float a) {
    return sqrt(max0(a));
}

//--// Intersections //-------------------------------------------------------//

float DistanceToTopAtmosphereBoundary(
    //AtmosphereParameters atmosphere,
    float r,
    float mu
    ) {
        float discriminant = r * r * (mu * mu - 1.0) + atmosphere_top_radius_sq;
        return max0(-r * mu + SafeSqrt(discriminant));
}

float DistanceToBottomAtmosphereBoundary(
    //AtmosphereParameters atmosphere,
    float r,
    float mu
    ) {
        float discriminant = r * r * (mu * mu - 1.0) + atmosphere_bottom_radius_sq;
        return max0(-r * mu - SafeSqrt(discriminant));
}

bool RayIntersectsGround(
    //AtmosphereParameters atmosphere,
    float r,
    float mu
    ) {
        return mu < 0.0 && r * r * (mu * mu - 1.0) + atmosphere_bottom_radius_sq >= 0.0;
}

//--// Coord Transforms //----------------------------------------------------//

float GetTextureCoordFromUnitRange(float x, float texture_size) {
    return 0.5 / texture_size + x * oneMinus(1.0 / texture_size);
}

//--// Transmittance Lookup //------------------------------------------------//

vec2 GetTransmittanceTextureUvFromRMu(
    //AtmosphereParameters atmosphere,
    float r,
    float mu
    ) {
        // Distance to top atmosphere boundary for a horizontal ray at ground level.
        const float H = sqrt(atmosphere_top_radius_sq - atmosphere_bottom_radius_sq);

        // Distance to the horizon.
        float rho = SafeSqrt(r * r - atmosphere_bottom_radius_sq);

        // Distance to the top atmosphere boundary for the ray (r,mu), and its minimum
        // and maximum values over all mu - obtained for (r,1) and (r,mu_horizon).
        float d = DistanceToTopAtmosphereBoundary(r, mu);
        float d_min = atmosphere_top_radius - r;
        float d_max = rho + H;
        //float x_mu = (d - d_min) / (d_max - d_min);
        //float x_r = rho / H;
        return vec2(GetTextureCoordFromUnitRange((d - d_min) / (d_max - d_min), TRANSMITTANCE_TEXTURE_WIDTH),
                    GetTextureCoordFromUnitRange(rho / H, TRANSMITTANCE_TEXTURE_HEIGHT));
}

vec3 GetTransmittanceToTopAtmosphereBoundary(
    //AtmosphereParameters atmosphere,
    float r,
    float mu
    ) {
        vec2 uv = GetTransmittanceTextureUvFromRMu(r, mu);
	    uv = clamp(uv, vec2(0.5 / 256.0, 0.5 / 64.0), vec2(255.5 / 256.0, 63.5 / 64.0));
        return vec3(texture(colortex4, vec3(uv * vec2(1.0, 0.5), 32.5 / 33.0)));
}

vec3 GetTransmittance(
    //AtmosphereParameters atmosphere,
    float r,
    float mu,
    float d,
    bool ray_r_mu_intersects_ground
    ) {
        float r_d = ClampRadius(sqrt(d * d + 2.0 * r * mu * d + r * r));
        float mu_d = ClampCosine((r * mu + d) / r_d);

        if (ray_r_mu_intersects_ground) {
            return min(
                GetTransmittanceToTopAtmosphereBoundary(r_d, -mu_d) /
                GetTransmittanceToTopAtmosphereBoundary(r, -mu),
            vec3(1.0));
        } else {
            return min(
                GetTransmittanceToTopAtmosphereBoundary(r, mu) /
                GetTransmittanceToTopAtmosphereBoundary(r_d, mu_d),
            vec3(1.0));
        }
}

vec3 GetTransmittance(vec3 view_ray) {
	vec3 camera = vec3(0.0, planetRadius + eyeAltitude, 0.0);
    // Compute the distance to the top atmosphere boundary along the view ray,
    // assuming the viewer is in space (or NaN if the view ray does not intersect
    // the atmosphere).
    float r = length(camera);
    float rmu = dot(camera, view_ray);
    float distance_to_top_atmosphere_boundary = -rmu - sqrt(rmu * rmu - r * r + atmosphere_top_radius_sq);

    // If the viewer is in space and the view ray intersects the atmosphere, move
    // the viewer to the top atmosphere boundary (along the view ray):
    if (distance_to_top_atmosphere_boundary > 0.0) {
        camera += view_ray * distance_to_top_atmosphere_boundary;
        r = atmosphere_top_radius;
        rmu += distance_to_top_atmosphere_boundary;
    } else if (r > atmosphere_top_radius) {
        // If the view ray does not intersect the atmosphere, simply return 0.
        return vec3(1.0);
    }

    // Compute the r, mu, mu_s and nu parameters needed for the texture lookups.
    float mu = rmu / r;

	return GetTransmittanceToTopAtmosphereBoundary(r, mu);
}

// vec3 GetTransmittance(vec3 ray_origin, vec3 ray_dir) {
// 	float r_sq = dot(ray_origin, ray_origin);
// 	float rcp_r = inversesqrt(r_sq);
// 	float mu = dot(ray_origin, ray_dir) * rcp_r;
// 	float r = r_sq * rcp_r;

// 	return GetTransmittanceToTopAtmosphereBoundary(mu, r);
// }

vec3 GetTransmittanceToSun(
    // AtmosphereParameters atmosphere,
    float r,
    float mu_s
    ) {
        float sin_theta_h = atmosphere_bottom_radius / r;
        float cos_theta_h = -sqrt(max0(1.0 - sin_theta_h * sin_theta_h));

        return GetTransmittanceToTopAtmosphereBoundary(r, mu_s) *
            smoothstep(-sin_theta_h * sun_angular_radius,
                        sin_theta_h * sun_angular_radius,
                        mu_s - cos_theta_h);
}

//--// Scattering Lookup //---------------------------------------------------//

vec4 GetScatteringTextureUvwzFromRMuMuSNu(
    //AtmosphereParameters atmosphere,
    float r,
    float mu,
    float mu_s,
    float nu,
    bool ray_r_mu_intersects_ground
    ) {
        // Distance to top atmosphere boundary for a horizontal ray at ground level.
        float H = sqrt(atmosphere_top_radius_sq - atmosphere_bottom_radius_sq);

        // Distance to the horizon.
        float rho = SafeSqrt(r * r - atmosphere_bottom_radius_sq);
        float u_r = GetTextureCoordFromUnitRange(rho / H, SCATTERING_TEXTURE_R_SIZE);

        // Discriminant of the quadratic equation for the intersections of the ray
        // (r,mu) with the ground (see RayIntersectsGround).
        float r_mu = r * mu;
        float discriminant = r_mu * r_mu - r * r + atmosphere_bottom_radius_sq;
        float u_mu;

        if (ray_r_mu_intersects_ground) {
            // Distance to the ground for the ray (r,mu), and its minimum and maximum
            // values over all mu - obtained for (r,-1) and (r,mu_horizon).
            float d = -r_mu - SafeSqrt(discriminant);
            float d_min = r - atmosphere_bottom_radius;
            float d_max = rho;
            u_mu = 0.5 - 0.5 * GetTextureCoordFromUnitRange(d_max == d_min ? 0.0 : (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE * 0.5);
        }else{
            // Distance to the top atmosphere boundary for the ray (r,mu), and its
            // minimum and maximum values over all mu - obtained for (r,1) and
            // (r,mu_horizon).
            float d = -r_mu + SafeSqrt(discriminant + H * H);
            float d_min = atmosphere_top_radius - r;
            float d_max = rho + H;
            u_mu = 0.5 + 0.5 * GetTextureCoordFromUnitRange((d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE * 0.5);
        }

        float d = DistanceToTopAtmosphereBoundary(atmosphere_bottom_radius, mu_s);
        float d_min = atmosphere_top_radius - atmosphere_bottom_radius;
        float d_max = H;
        float a = (d - d_min) / (d_max - d_min);
        float D = DistanceToTopAtmosphereBoundary(atmosphere_bottom_radius, mu_s_min);
        float A = (D - d_min) / (d_max - d_min);
        // An ad-hoc function equal to 0 for mu_s = mu_s_min (because then d = D and
        // thus a = A), equal to 1 for mu_s = 1 (because then d = d_min and thus
        // a = 0), and with a large slope around mu_s = 0, to get more texture 
        // samples near the horizon.
        float u_mu_s = GetTextureCoordFromUnitRange(max0(1.0 - a / A) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);
        float u_nu = nu * 0.5 + 0.5;
        return vec4(u_nu, u_mu_s, u_mu, u_r);
}

vec3 GetExtrapolatedSingleMieScattering(
    AtmosphereParameters atmosphere,
    vec4 scattering
    ) {
        // Algebraically this can never be negative, but rounding errors can produce
        // that effect for sufficiently short view rays.
        if (scattering.r <= 0.0) {
            return vec3(0.0);
        }
        return scattering.rgb * scattering.a / scattering.r *
            (atmosphere.rayleigh_scattering.r / atmosphere.mie_scattering.r) *
            (atmosphere.mie_scattering / atmosphere.rayleigh_scattering);
}

vec3 GetCombinedScattering(
    AtmosphereParameters atmosphere,
    float r,
    float mu,
    float mu_s,
    float nu,
    bool ray_r_mu_intersects_ground,
    out vec3 single_mie_scattering
    ) {
        vec4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(r, mu, mu_s, nu, ray_r_mu_intersects_ground);
        float tex_coord_x = uvwz.x * (SCATTERING_TEXTURE_NU_SIZE - 1.0);
        float tex_x = floor(tex_coord_x);
        float lerp = tex_coord_x - tex_x;
        vec3 uvw0 = vec3((tex_x + uvwz.y) / SCATTERING_TEXTURE_NU_SIZE, uvwz.z, uvwz.w);
        vec3 uvw1 = vec3((tex_x + 1.0 + uvwz.y) / SCATTERING_TEXTURE_NU_SIZE, uvwz.z, uvwz.w);

        vec4 combined_scattering = texture(colortex4, uvw0) * oneMinus(lerp) + texture(colortex4, uvw1) * lerp;

        vec3 scattering = vec3(combined_scattering);
        single_mie_scattering = GetExtrapolatedSingleMieScattering(atmosphere, combined_scattering);

        return scattering;
}

//--// Irradiance Lookup //---------------------------------------------------//

vec3 GetIrradiance(
    //AtmosphereParameters atmosphere,
    float r,
    float mu_s
    ) {
        float x_r = (r - atmosphere_bottom_radius) / (atmosphere_top_radius - atmosphere_bottom_radius);
        float x_mu_s = mu_s * 0.5 + 0.5;
        vec2 uv = vec2(GetTextureCoordFromUnitRange(x_mu_s, IRRADIANCE_TEXTURE_WIDTH),
                       GetTextureCoordFromUnitRange(x_r, IRRADIANCE_TEXTURE_HEIGHT));
	    uv = clamp(uv, vec2(0.5 / 64.0, 0.5 / 16.0), vec2(63.5 / 64.0, 15.5 / 16.0));

        return vec3(texture(colortex4, vec3(uv * vec2(0.25, 0.125) + vec2(0.0, 0.5), 32.5 / 33.0)));
}

//--// Rendering //-----------------------------------------------------------//

vec3 GetSkyRadiance(
    AtmosphereParameters atmosphere,
    vec3 view_ray,
    vec3 sun_direction,
    out vec3 transmittance
    ) {
		vec3 camera = vec3(0.0, planetRadius + eyeAltitude, 0.0);
        // Compute the distance to the top atmosphere boundary along the view ray,
        // assuming the viewer is in space (or NaN if the view ray does not intersect
        // the atmosphere).
        float r = length(camera);
        float rmu = dot(camera, view_ray);
        float distance_to_top_atmosphere_boundary = -rmu - sqrt(rmu * rmu - r * r + atmosphere_top_radius_sq);

        // If the viewer is in space and the view ray intersects the atmosphere, move
        // the viewer to the top atmosphere boundary (along the view ray):
        if (distance_to_top_atmosphere_boundary > 0.0) {
            camera += view_ray * distance_to_top_atmosphere_boundary;
            r = atmosphere_top_radius;
            rmu += distance_to_top_atmosphere_boundary;
        } else if (r > atmosphere_top_radius) {
            // If the view ray does not intersect the atmosphere, simply return 0.
            transmittance = vec3(1.0);
            return vec3(0.0);
        }

        // Compute the r, mu, mu_s and nu parameters needed for the texture lookups.
        float mu = rmu / r;
        float mu_s = dot(camera, sun_direction) / r;
        float nu = dot(view_ray, sun_direction);

        bool ray_r_mu_intersects_ground = RayIntersectsGround(r, mu);

        transmittance = ray_r_mu_intersects_ground ? vec3(0.0) : GetTransmittanceToTopAtmosphereBoundary(r, mu);

        vec3 sun_single_mie_scattering;
        vec3 sun_scattering;

        vec3 moon_single_mie_scattering;
        vec3 moon_scattering;

        vec3 groundDiffuse = vec3(0.0);
        #ifdef SKY_GROUND
            if (ray_r_mu_intersects_ground) {
                vec3 planet_surface = camera + view_ray * DistanceToBottomAtmosphereBoundary(r, mu);

                float r = length(planet_surface);
                float mu_s = dot(planet_surface, sun_direction) / r;

                vec3 sky_irradiance = GetIrradiance(r, mu_s);
                sky_irradiance += GetIrradiance(r, -mu_s) * MoonFlux;
                vec3 sun_irradiance = atmosphere.solar_irradiance * GetTransmittanceToSun(r, mu_s);

                float d = distance(camera, planet_surface);
                vec3 surface_transmittance = GetTransmittance(r, mu, d, ray_r_mu_intersects_ground);

                groundDiffuse = mix(sky_irradiance * 0.1, sun_irradiance * 0.01, wetness * 0.7) * surface_transmittance;
            }
        #else
            ray_r_mu_intersects_ground = false;
        #endif

        sun_scattering = GetCombinedScattering(atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground, sun_single_mie_scattering);
        moon_scattering = GetCombinedScattering(atmosphere, r, mu, -mu_s, -nu, ray_r_mu_intersects_ground, moon_single_mie_scattering);

        vec3 rayleigh = sun_scattering * RayleighPhase(nu)
                     + moon_scattering * RayleighPhase(-nu) * MoonFlux;

        vec3 mie = sun_single_mie_scattering * HenyeyGreensteinPhase(nu, mie_phase_g)
                + moon_single_mie_scattering * HenyeyGreensteinPhase(-nu, mie_phase_g) * MoonFlux;

        rayleigh = mix(rayleigh,  GetLuminance(rayleigh) * vec3(1.026186824, 0.9881671071, 1.015787125), wetness * 0.7);

        return (rayleigh + mie + groundDiffuse) * oneMinus(wetness * 0.6);
}

// vec3 GetSkyRadiance(
//     AtmosphereParameters atmosphere,
//     vec3 view_ray,
//     vec3 sun_direction
//     ) {
// 		vec3 camera = vec3(0.0, planetRadius + eyeAltitude, 0.0);
//         // Compute the distance to the top atmosphere boundary along the view ray,
//         // assuming the viewer is in space (or NaN if the view ray does not intersect
//         // the atmosphere).
//         float r = length(camera);
//         float rmu = dot(camera, view_ray);
//         float distance_to_top_atmosphere_boundary = -rmu - sqrt(rmu * rmu - r * r + atmosphere_top_radius_sq);

//         // If the viewer is in space and the view ray intersects the atmosphere, move
//         // the viewer to the top atmosphere boundary (along the view ray):
//         if (distance_to_top_atmosphere_boundary > 0.0) {
//             camera += view_ray * distance_to_top_atmosphere_boundary;
//             r = atmosphere_top_radius;
//             rmu += distance_to_top_atmosphere_boundary;
//         } else if (r > atmosphere_top_radius) {
//             // If the view ray does not intersect the atmosphere, simply return 0.
//             return vec3(0.0);
//         }

//         // Compute the r, mu, mu_s and nu parameters needed for the texture lookups.
//         float mu = rmu / r;
//         float mu_s = dot(camera, sun_direction) / r;
//         float nu = dot(view_ray, sun_direction);

//         bool ray_r_mu_intersects_ground = RayIntersectsGround(r, mu);

//         vec3 groundDiffuse = vec3(0.0);
//         #ifdef SKY_GROUND
//             if (ray_r_mu_intersects_ground) {
//                 vec3 planet_surface = camera + view_ray * DistanceToBottomAtmosphereBoundary(r, mu);

//                 float r = length(planet_surface);
//                 float mu_s = dot(planet_surface, sun_direction) / r;

//                 vec3 sky_irradiance = GetIrradiance(r, mu_s);
//                 sky_irradiance += GetIrradiance(r, -mu_s) * MoonFlux;
//                 vec3 sun_irradiance = atmosphere.solar_irradiance * GetTransmittanceToSun(r, mu_s);

//                 float d = distance(camera, planet_surface);
//                 vec3 surface_transmittance = GetTransmittance(r, mu, d, ray_r_mu_intersects_ground);

//                 groundDiffuse = mix(sky_irradiance * 0.1, sun_irradiance * 0.01, wetness * 0.7) * surface_transmittance;
//             }
//         #else
//             ray_r_mu_intersects_ground = false;
//         #endif

//         vec3 sun_single_mie_scattering;
//         vec3 sun_scattering = GetCombinedScattering(atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground, sun_single_mie_scattering);

//         vec3 moon_single_mie_scattering;
//         vec3 moon_scattering = GetCombinedScattering(atmosphere, r, mu, -mu_s, -nu, ray_r_mu_intersects_ground, moon_single_mie_scattering);

//         vec3 rayleigh = sun_scattering * RayleighPhase(nu)
//                      + moon_scattering * RayleighPhase(-nu) * MoonFlux;

//         vec3 mie = sun_single_mie_scattering * HenyeyGreensteinPhase(nu, mie_phase_g)
//                 + moon_single_mie_scattering * HenyeyGreensteinPhase(-nu, mie_phase_g) * MoonFlux;

//         rayleigh = mix(rayleigh,  GetLuminance(rayleigh) * vec3(1.026186824, 0.9881671071, 1.015787125), wetness * 0.7);

//         return (rayleigh + mie + groundDiffuse) * oneMinus(wetness * 0.6);
// }

vec3 GetSkyRadianceToPoint(
    AtmosphereParameters atmosphere,
    //vec3 camera,
    vec3 point,
    vec3 sun_direction,
    out vec3 transmittance
    ) {
		vec3 camera = vec3(0.0, planetRadius + eyeAltitude, 0.0);
        // Compute the distance to the top atmosphere boundary along the view ray,
        // assuming the viewer is in space (or NaN if the view ray does not intersect
        // the atmosphere).
        vec3 view_ray = normalize(point);
        float r = length(camera);
        float rmu = dot(camera, view_ray);
        float distance_to_top_atmosphere_boundary = -rmu - sqrt(rmu * rmu - r * r + atmosphere_top_radius_sq);

        // If the viewer is in space and the view ray intersects the atmosphere, move
        // the viewer to the top atmosphere boundary (along the view ray):
        if (distance_to_top_atmosphere_boundary > 0.0) {
            camera += view_ray * distance_to_top_atmosphere_boundary;
            r = atmosphere_top_radius;
            rmu += distance_to_top_atmosphere_boundary;
        }

        // Compute the r, mu, mu_s and nu parameters for the first texture lookup.
        float mu = rmu / r;
        float mu_s = dot(camera, sun_direction) / r;
        float nu = dot(view_ray, sun_direction);
        float d = length(point);
        bool ray_r_mu_intersects_ground = RayIntersectsGround(r, mu);

        transmittance = GetTransmittance(r, mu, d, ray_r_mu_intersects_ground);

        vec3 sun_single_mie_scattering;
        vec3 sun_scattering = GetCombinedScattering(atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground, sun_single_mie_scattering);
        vec3 moon_single_mie_scattering;
        vec3 moon_scattering = GetCombinedScattering(atmosphere, r, mu, -mu_s, -nu, ray_r_mu_intersects_ground, moon_single_mie_scattering);

        // Compute the r, mu, mu_s and nu parameters for the second texture lookup.
        // If shadow_length is not 0 (case of light shafts), we want to ignore the
        // scattering along the last shadow_length meters of the view ray, which we
        // do by subtracting shadow_length from d (this way scattering_p is equal to
        // the S|x_s=x_0-lv term in Eq. (17) of our paper).
        float r_p = ClampRadius(sqrt(d * d + 2.0 * r * mu * d + r * r));
        float mu_p = (r * mu + d) / r_p;
        float mu_s_p = (r * mu_s + d * nu) / r_p;
        float mu_s_p_m = (r * -mu_s + d * -nu) / r_p;

        vec3 sun_single_mie_scattering_p;
        vec3 sun_scattering_p = GetCombinedScattering(atmosphere, r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground, sun_single_mie_scattering_p);
        vec3 moon_single_mie_scattering_p;
        vec3 moon_scattering_p = GetCombinedScattering(atmosphere, r_p, mu_p, mu_s_p_m, -nu, ray_r_mu_intersects_ground, moon_single_mie_scattering_p);

        sun_scattering -= transmittance * sun_scattering_p;
        sun_single_mie_scattering -= transmittance * sun_single_mie_scattering_p;
        moon_scattering = moon_scattering - transmittance * moon_scattering_p;
        moon_single_mie_scattering -= transmittance * moon_single_mie_scattering_p;

        // Hack to avoid rendering artifacts when the sun is below the horizon.
        sun_single_mie_scattering *= smoothstep(0.0, 0.01, mu_s);
        moon_single_mie_scattering *= smoothstep(0.0, 0.01, -mu_s);

        vec3 rayleigh = sun_scattering * RayleighPhase(nu)
                     + moon_scattering * RayleighPhase(-nu) * MoonFlux;

        vec3 mie = sun_single_mie_scattering * HenyeyGreensteinPhase(nu, mie_phase_g)
                + moon_single_mie_scattering * HenyeyGreensteinPhase(-nu, mie_phase_g) * MoonFlux;

        rayleigh = mix(rayleigh,  GetLuminance(rayleigh) * vec3(1.026186824, 0.9881671071, 1.015787125), wetness * 0.7);

        return (rayleigh + mie) * oneMinus(wetness * 0.6);
}

// vec3 GetSunAndSkyIrradiance(
//     AtmosphereParameters atmosphere,
//     vec3 point,
//     vec3 sun_direction,
//     out vec3 sky_irradiance
//     ) {
//         float r = length(point);
//         float mu_s = dot(point, sun_direction) / r;

//         sky_irradiance = GetIrradiance(r, mu_s) + GetIrradiance(r, -mu_s) * MoonFlux;
//         sky_irradiance *= 1.0 + point.y / r;

//         vec3 sun_irradiance  = GetTransmittanceToSun(r, mu_s);
//         vec3 moon_irradiance = GetTransmittanceToSun(r, -mu_s) * MoonFlux;

//         return atmosphere.solar_irradiance * (sun_irradiance + DoNightEye(moon_irradiance));
// }

vec3 GetSunAndSkyIrradiance(
    AtmosphereParameters atmosphere,
    vec3 point,
    vec3 sun_direction,
    out vec3 sun_irradiance,
    out vec3 moon_irradiance
    ) {
        float r = length(point);
        float mu_s = dot(point, sun_direction) / r;

        sun_irradiance = atmosphere.solar_irradiance * GetTransmittanceToSun(r, mu_s);
        moon_irradiance = atmosphere.solar_irradiance * DoNightEye(GetTransmittanceToSun(r, -mu_s) * MoonFlux);

        vec3 sky_irradiance = GetIrradiance(r, mu_s) + GetIrradiance(r, -mu_s) * MoonFlux;
        sky_irradiance *= 1.0 + point.y / r;

        return sky_irradiance;
}

#endif

#define coneAngleToSolidAngle(x) (TAU * oneMinus(cos(x)))

vec3 RenderSun(in vec3 worldDir, in vec3 sunVector) {
	//const float sunRadius = 1392082.56;
	//const float sunDist = 149.6e6;

	//const float sunAngularRadius = sunRadius / sunDist * 0.5;
	// const float sunAngularRadius = 1e-2;

	//const vec3 sunIlluminance = vec3(1.0, 0.973, 0.961) * 126.6e3;
	const vec3 sunIlluminance = vec3(1.474000,1.850400,1.911980) * 126.6e3;

    float cosTheta = dot(worldDir, sunVector);
    float centerToEdge = saturate(fastAcos(cosTheta) / sun_angular_radius);
    if (cosTheta < cos(sun_angular_radius)) return vec3(0.0);

	const vec3 alpha = vec3(0.429, 0.522, 0.614); // for AP1 primaries

    vec3 factor = pow(vec3(1.0 - centerToEdge * centerToEdge), alpha * 0.5);
    vec3 finalLuminance = sunIlluminance / coneAngleToSolidAngle(sun_angular_radius) * factor;

	//float visibility = curve(saturate(worldDir.y * 30.0));

    return min(finalLuminance, 1e4);
}

vec3 RenderSunReflection(in vec3 worldDir, in vec3 sunVector) {
	const vec3 sunIlluminance = vec3(1.474000,1.850400,1.911980) * 126.6e3;

    float cosTheta = dot(worldDir, sunVector);
    float centerToEdge = saturate(fastAcos(cosTheta) / 0.05);
    if (cosTheta < cos(0.05)) return vec3(0.0);

	const vec3 alpha = vec3(0.429, 0.522, 0.614); // for AP1 primaries

    vec3 factor = pow(vec3(1.0 - centerToEdge * centerToEdge), alpha * 0.5);
    vec3 finalLuminance = sunIlluminance / coneAngleToSolidAngle(0.05) * factor;

    return min(finalLuminance, 2e3);
}

vec3 RenderMoonReflection(in vec3 worldDir, in vec3 sunVector) {
	float cosTheta = dot(worldDir, -sunVector);

	float size = 5e-3;
	float hardness = 2e2;

	float disc = sqr(curve(saturate((cosTheta - 1.0 + size) * hardness)));

	return vec3(disc) * 4.0;
}

vec3 RenderStars(in vec3 worldDir) {
	const float scale = 256.0;
	const float coverage = 0.1 * STARS_COVERAGE;
	const float maxLuminance = 0.6 * STARS_INTENSITY;
	const float minTemperature = 4000.0;
	const float maxTemperature = 8000.0;

	//float visibility = curve(saturate(worldDir.y));

	float cosine = worldSunVector.z;
	vec3 axis = cross(worldSunVector, vec3(0.0, 0.0, 1.0));
	float cosecantSquared = rcp(dotSelf(axis));
	worldDir = cosine * worldDir + cross(axis, worldDir) + cosecantSquared * oneMinus(cosine) * dot(axis, worldDir) * axis;

	vec3  p = worldDir * scale;
	ivec3 i = ivec3(floor(p));
	vec3  f = p - i;
	float r = dotSelf(f - 0.5);

	vec3 i3 = fract(i * vec3(443.897, 441.423, 437.195));
	i3 += dot(i3, i3.yzx + 19.19);
	vec2 hash = fract((i3.xx + i3.yz) * i3.zy);
	hash.y = 2.0 * hash.y - 4.0 * hash.y * hash.y + 3.0 * hash.y * hash.y * hash.y;

	float cov = remap(oneMinus(coverage), 1.0, hash.x);
	return maxLuminance * remap(0.25, 0.0, r) * cov * cov * Blackbody(mix(minTemperature, maxTemperature, hash.y));
}
