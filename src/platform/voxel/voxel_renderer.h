#ifndef GUARD_VOXEL_RENDERER_H
#define GUARD_VOXEL_RENDERER_H

#ifdef PLATFORM_SDL2
#if defined(NATIVE_LINUX) || defined(__ANDROID__)

#include <stdbool.h>

// Initialize the voxel renderer. Call after OpenGL context is created.
// Returns true on success, false on failure.
bool VoxelRenderer_Init(void);

// Render one frame of the 3D voxel world.
void VoxelRenderer_RenderFrame(void);

// Shut down the voxel renderer and free resources.
void VoxelRenderer_Shutdown(void);

// Queue a camera adjustment (degrees / zoom factor) from another thread, e.g.
// the Android touch overlay. Applied on the next rendered frame.
void VoxelRenderer_TouchCamera(float dYaw, float dPitch, float zoomFactor);

// External flag set by main() from command-line args
extern bool gVoxelModeEnabled;
extern bool gVoxelFirstPersonMode;

#endif // NATIVE_LINUX || __ANDROID__
#endif // PLATFORM_SDL2
#endif // GUARD_VOXEL_RENDERER_H
