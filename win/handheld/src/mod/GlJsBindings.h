#ifndef NET_MINECRAFT_MOD__GlJsBindings_H__
#define NET_MINECRAFT_MOD__GlJsBindings_H__

// JS bindings for the render-mod API:
//   - global `GL`:  raw desktop-OpenGL shader/texture/FBO/draw calls +
//                   GL_* numeric constants (mods write their own GLSL)
//   - global `Render`: offscreen scene switch + scene texture + camera
//                      snapshot (camera API seed for mirror/water work)
// Desktop GL only (the MCPE-Win build); on GLES platforms nothing is
// registered and Render.enable() stays a no-op.

#include "../../thirdparty/duktape/duktape.h"

#if defined(_WIN32) || defined(MACOS) || defined(LINUX)
void registerGlJsBindings(duk_context* ctx);
#else
static inline void registerGlJsBindings(duk_context* ctx) { (void)ctx; }
#endif

#endif /*NET_MINECRAFT_MOD__GlJsBindings_H__*/
