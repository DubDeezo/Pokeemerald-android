#ifndef VOXEL_MODELS_H
#define VOXEL_MODELS_H

#ifdef PLATFORM_SDL2
#if defined(NATIVE_LINUX) || defined(__ANDROID__)

#include "gl_compat.h"

extern GLuint gVoxelModels[1024];

#endif
#endif
#endif
