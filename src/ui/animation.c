/*
 * animation.c - frame-rate independent smoothing helpers.
 */

#include "ui/animation.h"
#include <math.h>

float anim_clamp(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float anim_approach(float current, float target, float dt_ms, float tau_ms)
{
    if (tau_ms <= 0.0f || dt_ms <= 0.0f) return target;
    /* Exponential smoothing: converges in ~3*tau regardless of framerate. */
    float k = 1.0f - expf(-dt_ms / tau_ms);
    float next = current + (target - current) * k;
    if (fabsf(target - next) < 0.001f) return target;
    return next;
}

float anim_ease_smooth(float t)
{
    t = anim_clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float anim_ease_out_cubic(float t)
{
    t = anim_clamp(t, 0.0f, 1.0f);
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}
