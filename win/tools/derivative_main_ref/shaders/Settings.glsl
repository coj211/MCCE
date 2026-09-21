#if !defined INCLUDE_SETTINGS
#define INCLUDE_SETTINGS

//--// GI & AO //-------------------------------------------------------------//

//#define GI_ENABLED // Indirect lighting from sunlight.
//#define GI_RENDER_RESOLUTION 0.5 // [0.1 0.2 0.3 0.4 0.5]
#define GI_SAMPLES 16 // [4 8 12 16 20 24 32 48 64 96 128 256]
#define GI_RADIUS 30 // [2 4 6 8 10 15 20 25 30 35 40 45 50 60 70 80 90 100 120 150 170 200 250 300 500]
#define GI_BRIGHTNESS 1.0 // [0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.6 1.8 2.0 2.5 3.0 5.0 7.0 10.0 15.0 20.0 30.0 40.0 50.0 70.0 100.0]

#if defined IS_NETHER
	#undef GI_ENABLED
#endif

#define SSAO_ENABLED // Screen space ambient occlusion.
#define SSAO_SAMPLES 6 // [1 2 3 4 5 6 7 8 9 10 12 16 18 20 22 24 26 28 30 32 48 64]
#define SSAO_STRENGTH 1.0 // [0.05 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.7 2.0 2.5 3.0 4.0 5.0 7.0 10.0]

//--// Atmospherics //--------------------------------------------------------//

const ivec2 skyCaptureRes = ivec2(255, 256);

#define TEMPORAL_UPSCALING 2 // [2 3 4]
#define MAX_BLENDED_FRAMES 40.0 // [8.0 12.0 16.0 20.0 24.0 28.0 32.0 36.0 40.0 48.0 56.0 64.0 72.0 80.0 96.0 112.0 128.0 144.0 160.0 192.0 224.0 256.0]

#define PLANAR_CLOUDS
#define VOLUMETRIC_CLOUDS

#define CIRRUS_CLOUDS 1 // [0 1 2]
#define CIRROCUMULUS_CLOUDS

#define CLOUDS_SPEED 1.0 // [0.0 1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0 15.0 20.0 25.0 30.0 40.0 50.0 60.0 70.0 80.0 90.0 100.0 120.0 150.0 170.0 200.0]
#define minTransmittance 0.05

//#define CLOUDS_SHADOW

#define PC_SHADOW
#define VC_SHADOW

#ifndef PLANAR_CLOUDS
	#undef PC_SHADOW
#endif
#ifndef VOLUMETRIC_CLOUDS
	#undef VC_SHADOW
#endif

#if !defined PC_SHADOW && !defined VC_SHADOW
	#undef CLOUDS_SHADOW
#endif

#define CLOUDS_WEATHER

//#define AURORA
#define AURORA_STRENGTH 0.7 // [0.05 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.7 2.0 2.5 3.0 4.0 5.0 7.0 10.0 15.0 20.0 30.0 50.0]

#define STARS_INTENSITY 0.1  // [0.0 0.01 0.02 0.03 0.04 0.05 0.06 0.07 0.08 0.09 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.65 0.7 0.75 0.8 0.85 0.9 0.95 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0]
#define STARS_COVERAGE  0.15 // [0.0 0.01 0.02 0.03 0.04 0.05 0.06 0.07 0.08 0.09 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.65 0.7 0.75 0.8 0.85 0.9 0.95 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0]

#define LAND_ATMOSPHERIC_SCATTERING

#define VOLUMETRIC_FOG
#define VOLUMETRIC_LIGHT
#define UW_VOLUMETRIC_LIGHT

#if defined VOLUMETRIC_FOG || defined VOLUMETRIC_LIGHT
	#undef LAND_ATMOSPHERIC_SCATTERING
#endif

#define FOG_TYPE 1 // [0 1 2 3]

//--// Shadows //-------------------------------------------------------------//

#define SHADOW_MAP_BIAS	0.9 // [0.0 0.1 0.7 0.75 0.8 0.85 0.9 0.95 1.0]

#define COLORED_SHADOWS // Colored shadows from stained glass.

#define SCREEN_SPACE_SHADOWS

//#define SHADOW_BACKFACE_CULLING

//#define DH_SHADOW

#ifdef DH_SHADOW
#endif

//--// Lighting //------------------------------------------------------------//

#define SUNLIGHT_INTENSITY 1.0 // Intensity of sunlight. [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0]
#define SKYLIGHT_INTENSITY 1.0	// Intensity of skylight. [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0]
#define NIGHT_BRIGHTNESS 0.0005 // [0.0 0.00005 0.00007 0.0001 0.0002 0.0003 0.0005 0.0006 0.0007 0.0008 0.0009 0.001 0.0015 0.002 0.0025 0.003 0.004 0.005 0.006 0.007 0.01 0.05 1.0]

#define TORCHLIGHT_BRIGHTNESS 1.0 // Brightness of torch light. [0.5 1.0 2.0 3.0 4.0 5.0 7.0 10.0]
#define TORCHLIGHT_COLOR_TEMPERATURE 3000 // Color temperature of torch light in Kelvin. [1000 1500 2000 2300 2500 3000 3400 3500 4000 4500 5000 5500 6000]

#define HELD_TORCHLIGHT // Holding an item with a light value will cast light into the scene when this is enabled. 
#define HELDLIGHT_BRIGHTNESS 1.0 // Brightness of held torch light. [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.2 1.5 1.7 2.0 2.5 3.0 3.5 4.0 4.5 5.0 7.0 10.0]

#define BASIC_BRIGHTNESS 0.0005 // [0.0 0.00001 0.000015 0.00002 0.00003 0.00005 0.0001 0.0002 0.0003 0.0005 0.0006 0.0007 0.0008 0.0009 0.001 0.0015 0.002 0.003 0.004 0.005 0.007 0.01 0.03 0.05 0.1 0.5 1.0]
#define BASIC_BRIGHTNESS_NETHER 0.06 // [0.0 0.0001 0.0002 0.0005 0.001 0.005 0.01 0.05 0.06 0.1 0.2 0.3 0.5 0.6 0.7 0.8 0.9 1.0 1.5 3.0 5.0]
#define BASIC_BRIGHTNESS_END 0.4 // [0.0 0.001 0.002 0.003 0.004 0.005 0.01 0.02 0.03 0.05 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.5 2.0 2.5 3.0 4.0 5.0 7.0 10.0]

//--// World //---------------------------------------------------------------//

#define WATER_REFRACT_IOR 1.33 // [0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.33 1.4 1.5 1.6]
#define WATER_CAUSTICS
#define WATER_FOG_DENSITY 1.0 // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.7 2.0 2.5 3.0 4.0 5.0 7.0 10.0]
#define WATER_WAVE_HEIGHT 1.0 // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 3.0 5.0 7.0 10.0]
#define WATER_WAVE_SPEED 1.0 // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.2 2.4 2.6 2.8 3.0 3.2 3.4 3.6 3.8 4.0 4.2 4.4 4.6 4.8 5.0 5.5 6.0 6.5 7.0 7.5 8.0 9.5 10.0]
#define WATER_PARALLAX

#define GLASS_REFRACT_IOR 1.5 // [0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 3.0 4.0 5.0 7.0 10.0 15.0]
#define GLASS_TEXTURE_ALPHA 1.0 // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 3.0 4.0 5.0 7.0 10.0]

//#define BORDER_FOG
#define BORDER_FOG_FALLOFF 8.0 // [1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0 11.0 12.0 13.0 14.0 15.0 16.0 18.0 20.0 22.0 24.0]

//#define WHITE_WORLD
#define DARK_END

//--// Surface //-------------------------------------------------------------//

#define TEXTURE_FORMAT 0 // [0 1]
#define ANISOTROPIC_FILTER 0 // [0 2 4 8 16 32 64]

//#define NORMAL_MAP
//#define SPECULAR_MAP

//#define POROSITY

//#define MOD_BLOCK_SUPPORT

#define SPECULAR_HIGHLIGHT_BRIGHTNESS 0.6 // Brightness of specular high light. [0.0 0.01 0.02 0.05 0.07 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 2.0 2.5 3.0 4.0 5.0 7.0 10.0 15.0]

#define SUBSERFACE_SCATTERING_MODE 0 // [0 1 2]
#define SUBSERFACE_SCATTERING_STRENTGH 1.0 // Brightness of subsurface scattering. [0.0 0.01 0.02 0.05 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 3.0 4.0 5.0 7.0 10.0 15.0]

#define ROUGH_REFLECTIONS
#define ROUGH_REFLECTIONS_THRESHOLD 0.005 // [0.0001 0.0002 0.0005 0.0007 0.001 0.002 0.005 0.007 0.01 0.02 0.05 0.07 0.1 0.2 0.5]

#define REFLECTION_FILTER

//#define PARALLAX // 3D effect for resource packs with heightmaps. Make sure Texture Resolution is set properly!
//#define TEXTURE_RESOLUTION 16 // Resolution of current resource pack. This needs to be set properly for POM! [16 32 64 128 256 512]
//#define SMOOTH_PARALLAX
#define PARALLAX_SHADOW // Self-shadowing for parallax occlusion mapping. 
//#define PARALLAX_BASED_NORMAL
#define PARALLAX_SAMPLES 60 // [10 20 30 40 50 60 70 80 90 100 120 150 200 250 300 400 500 600 700 1000]
#define PARALLAX_DEPTH 0.2 // Adjusts parallax deepness. [0.01 0.02 0.05 0.07 0.1 0.15 0.2 0.25 0.5 0.75 1.0 1.25 1.5 1.75 2.0 2.5 3.0 4.0 5.0 7.0 10.0]
#define PARALLAX_REFINEMENT
#define PARALLAX_REFINEMENT_STEPS 8 // [2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 18 24]

#ifndef NORMAL_MAP
	#undef MC_NORMAL_MAP
	//#undef PARALLAX
#endif

#ifndef SPECULAR_MAP
	#undef MC_SPECULAR_MAP
#endif

#ifndef PARALLAX
	#undef PARALLAX_SHADOW
	#undef PARALLAX_BASED_NORMAL
#endif

#define GENERAL_GRASS_FIX
#define FORCE_WET_EFFECT // Make all surfaces get wet during rain regardless of specular texture values
#define RAIN_SPLASH_EFFECT // Rain ripples/splashes on water and wet blocks.
//#define RAIN_SPLASH_BILATERAL // Bilateral filter for rain splash/ripples. When enabled, ripple texture is smoothed (no hard pixel edges) at the cost of performance.
#define ENTITY_STATUS_COLOR // Enables vanilla Minecraft entity color changing (red When hurt, creeper flashing When exploding). 
#define ENTITY_EYES_LIGHTING

//--// Post //----------------------------------------------------------------//

//#define DOF_ENABLED
#define FOCUSING_SPEED 6.0 // [1.0 1.5 2.0 2.5 3.0 3.5 4.0 4.5 5.0 5.5 6.0 6.5 7.0 7.5 8.0 8.5 9.0 9.5 10.0 11.0 12.0 14.0 16.0 20.0 24.0 28.0 30.0]

#define BLOOM_ENABLED
#define BLUR_SAMPLES 1 // [1 2 3 4 5 6]
#define BLOOM_AMOUNT 1.0 // [0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 3.0 4.0 5.0 7.0 10.0 15.0 20.0]
#define BLOOMY_FOG

//#define MOTION_BLUR // Motion blur. Makes motion look blurry.
#define MOTION_BLUR_SAMPLES 6 // [3 4 5 6 7 8 10 12 14 16 18 20 24]
#define MOTION_BLUR_STRENGTH 0.5 // [0.0 0.05 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.6 0.7 0.8 0.9 1.0 1.5 2.0 2.5 3.0 3.5 4.0 4.5 5.0 7.0 10.0]

#define TAA_ENABLED // Temporal Anti-Aliasing. Utilizes multiple rendered frames to reconstruct an anti-aliased image similar to supersampling. Can cause some artifacts.
//#define TAA_SHARPEN // Enables catmul rom filter
#define TAA_SHARPNESS 0.7 // [0.05 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.65 0.7 0.75 0.8 0.85 0.9 0.95 1.0]

#define AUTO_EXPOSURE
#define AUTO_EXPOSURE_LOD 6 // [1 2 3 4 5 6 7 8 9 10 11 12 14 16]
//#define AUTO_EXPOSURE_VALUE 0.7 // [0.1 0.2 0.3 0.4 0.5 0.6 0.65 0.7 0.75 0.8 0.85 0.9 0.95 1.0 1.5 2.0 3.0 4.0 5.0]
#define EXPOSURE_SPEED 1.0 // [0.1 0.2 0.3 0.5 0.7 1.0 1.3 1.6 2.0 2.5 3.0 5.0 7.0 10.0]
#define AUTO_EXPOSURE_BIAS 0.0 // [-2.0 -1.9 -1.8 -1.7 -1.6 -1.5 -1.4 -1.3 -1.2 -1.1 -1.0 -0.9 -0.8 -0.7 -0.6 -0.5 -0.4 -0.3 -0.2 -0.1 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0]
#define MANUAL_EXPOSURE_VALUE 12.0 // [0.1 0.3 0.5 1.0 1.5 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0 12.0 14.0 16.0 18.0 20.0 25.0 30.0 40.0 50.0]

#define CAS_ENABLED // Sharpens the final image (contrast-adaptive sharpening)
#define CAS_STRENGTH 0.3 // [0.0 0.01 0.02 0.03 0.04 0.05 0.06 0.07 0.08 0.09 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.65 0.7 0.75 0.8 0.85 0.9 0.95 1.0]

//--// Debug //---------------------------------------------------------------//

#define DEBUG_NORMAL 0 // [0 1 2]

#endif