/*
 * math_util.h - tiny portable math helpers shared by the procedural UI.
 *
 * Why this exists:
 *   The project builds with `-std=c11` (strict ISO C), which hides the
 *   POSIX constant M_PI unless _XOPEN_SOURCE/_DEFAULT_SOURCE is defined.
 *   Rather than sprinkling feature macros across translation units, we
 *   provide a single guarded definition here and include it wherever a
 *   bit of trigonometry is needed (boot screen, disc animations, ...).
 */
#ifndef DISCADIA_MATH_UTIL_H
#define DISCADIA_MATH_UTIL_H

#include <math.h>

/* Provide M_PI on strict-c11 toolchains that don't expose it by default. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Convert degrees to radians (handy for theme-driven rotation angles). */
static inline double deg2rad(double deg) { return deg * (M_PI / 180.0); }

/* Clamp helper used by animation/easing code. */
static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Linear interpolation between a and b by t in [0,1]. */
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

#endif /* DISCADIA_MATH_UTIL_H */
