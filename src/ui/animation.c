/*
 * animation.c - pure easing utilities (see animation.h). All functions
 * are branch-light and allocation-free: safe at 60fps on a Pi Zero.
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
    if (tau_ms <= 0.f) return target;
    /* exponential smoothing: x += (t-x)*(1-e^(-dt/tau)) */
    float k = 1.f - expf(-dt_ms / tau_ms);
    float next = current + (target - current) * k;
    if (fabsf(target - next) < 0.0005f) return target;
    return next;
}

float anim_ease_smooth(float t)
{
    t = anim_clamp(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);          /* smoothstep */
}

float anim_ease_out_cubic(float t)
{
    t = anim_clamp(t, 0.f, 1.f);
    float u = 1.f - t;
    return 1.f - u * u * u;
}

/* extra helpers used by boot/menu screens */
float anim_ease_out_quad(float t)
{
    t = anim_clamp(t, 0.f, 1.f);
    return 1.f - (1.f - t) * (1.f - t);
}

float anim_pingpong(float t01)
{
    /* triangle wave in [0..1): 0->1->0 over one period of t01*2 */
    float p = anim_clamp(t01, 0.f, 1.f);
    return p < 0.5f ? p * 2.f : 2.f - p * 2.f;
}
