#ifndef GUARD_VOXEL_DAYNIGHT_H
#define GUARD_VOXEL_DAYNIGHT_H

// Time-of-day lighting for the 3D overworld (Android).
//
// The world is multiplied by a tint colour that follows the device's local
// clock (blue-grey at night, warm at dawn and sunset, neutral by day), and the
// sky colour follows the same curve. The 2D screens and the text/menus drawn
// over the world are not tinted.

// Colour curve for a time of day, `minute` in [0, 1440). Pure function.
void VoxelDayNight_At(float minute, float outTint[3], float outSky[3]);

// The same, for the device's current local time (refreshed about once a second).
void VoxelDayNight_Get(float outTint[3], float outSky[3]);

#endif // GUARD_VOXEL_DAYNIGHT_H
