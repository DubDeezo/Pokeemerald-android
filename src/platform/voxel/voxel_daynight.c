// Time-of-day lighting for the 3D overworld. See voxel_daynight.h.
//
// The colour keys and the smoothstep blend between them are adapted from the
// day/night curve of Emerald 3DS (https://github.com/ZallaxDev/emerald-3ds,
// 3ds_port/src/3ds_daynight.c), MIT licence, Copyright (c) 2026 Zallax and
// Emerald 3DS contributors.

#include "voxel_daynight.h"

#if defined(PLATFORM_SDL2) && (defined(NATIVE_LINUX) || defined(__ANDROID__))

#include <time.h>

typedef struct
{
    float minute;
    float r, g, b;
    float daylight; // 0 = night, 1 = full day: how far the sky is from night blue
} DayNightKey;

static const DayNightKey kKeys[] = {
    {    0.0f, 0.66f, 0.74f, 0.92f, 0.00f },
    {  120.0f, 0.66f, 0.74f, 0.92f, 0.00f },
    {  240.0f, 0.96f, 0.94f, 0.88f, 0.75f },
    {  480.0f, 0.96f, 0.94f, 0.88f, 0.75f },
    {  600.0f, 1.00f, 1.00f, 1.00f, 1.00f },
    {  900.0f, 1.00f, 1.00f, 1.00f, 1.00f },
    { 1020.0f, 1.00f, 0.80f, 0.65f, 0.35f },
    { 1080.0f, 1.00f, 0.80f, 0.65f, 0.35f },
    { 1200.0f, 0.66f, 0.74f, 0.92f, 0.00f },
    { 1440.0f, 0.66f, 0.74f, 0.92f, 0.00f },
};
#define KEY_COUNT ((int)(sizeof(kKeys) / sizeof(kKeys[0])))

static const float kDaySky[3] = { 0.53f, 0.81f, 0.92f };
static const float kNightSky[3] = { 0.04f, 0.07f, 0.16f };

void VoxelDayNight_At(float minute, float outTint[3], float outSky[3])
{
    float tint[3] = { kKeys[0].r, kKeys[0].g, kKeys[0].b };
    float daylight = kKeys[0].daylight;

    if (minute < 0.0f)
        minute = 0.0f;
    if (minute > 1440.0f)
        minute = 1440.0f;

    for (int i = 1; i < KEY_COUNT; i++)
    {
        if (minute <= kKeys[i].minute)
        {
            const DayNightKey *a = &kKeys[i - 1];
            const DayNightKey *b = &kKeys[i];
            float span = b->minute - a->minute;
            float t = span > 0.0f ? (minute - a->minute) / span : 1.0f;

            t = t * t * (3.0f - 2.0f * t);
            tint[0] = a->r + (b->r - a->r) * t;
            tint[1] = a->g + (b->g - a->g) * t;
            tint[2] = a->b + (b->b - a->b) * t;
            daylight = a->daylight + (b->daylight - a->daylight) * t;
            break;
        }
    }

    for (int c = 0; c < 3; c++)
    {
        outTint[c] = tint[c];
        outSky[c] = (kNightSky[c] + (kDaySky[c] - kNightSky[c]) * daylight) * tint[c];
    }
}

void VoxelDayNight_Get(float outTint[3], float outSky[3])
{
    static time_t sLastSampled = 0;
    static float sTint[3] = { 1.0f, 1.0f, 1.0f };
    static float sSky[3] = { 0.53f, 0.81f, 0.92f };
    static int sValid = 0;
    time_t now = time(NULL);

    if (!sValid || now != sLastSampled)
    {
        struct tm local;

        if (localtime_r(&now, &local) != NULL)
        {
            float minute = (float)(local.tm_hour * 60 + local.tm_min) + (float)local.tm_sec / 60.0f;

            VoxelDayNight_At(minute, sTint, sSky);
            sValid = 1;
        }
        sLastSampled = now;
    }
    for (int c = 0; c < 3; c++)
    {
        outTint[c] = sTint[c];
        outSky[c] = sSky[c];
    }
}

#endif // PLATFORM_SDL2 && (NATIVE_LINUX || __ANDROID__)
