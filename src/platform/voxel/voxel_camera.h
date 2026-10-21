#ifndef GUARD_VOXEL_CAMERA_H
#define GUARD_VOXEL_CAMERA_H

#ifdef PLATFORM_SDL2
#if defined(NATIVE_LINUX) || defined(__ANDROID__)

typedef struct {
    float x, y, z;      // Camera position
    float targetX, targetY, targetZ; // Smooth follow target (player world pos)
    float pitch;        // degrees downward
    float yaw;          // horizontal rotation degrees
    float distance;     // distance from target
    float fov;          // field of view degrees
    float zoom;         // user zoom multiplier applied to distance (touch pinch)
} VoxelCamera;

void VoxelCamera_Init(VoxelCamera *cam);
void VoxelCamera_Update(VoxelCamera *cam, float playerWorldX, float playerWorldZ);
void VoxelCamera_FPSMouseMotion(VoxelCamera *cam, int dx, int dy);
void VoxelCamera_FPSTick(VoxelCamera *cam, float forward, float right, float up);
void VoxelCamera_ApplyProjection(const VoxelCamera *cam, int viewportW, int viewportH);
void VoxelCamera_ApplyView(const VoxelCamera *cam);
// Touch/user adjustment of the follow camera: yaw/pitch deltas in degrees and
// a multiplicative zoom factor. Values are clamped to keep the view usable.
void VoxelCamera_AdjustView(VoxelCamera *cam, float dYaw, float dPitch, float zoomFactor);

#endif // NATIVE_LINUX || __ANDROID__
#endif // PLATFORM_SDL2
#endif // GUARD_VOXEL_CAMERA_H
