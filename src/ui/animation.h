#ifndef DISCADIA_ANIMATION_H
#define DISCADIA_ANIMATION_H

/*
 * Tiny animation helpers: exponential smoothing and easing curves used
 * by the carousel, selection scaling and screen fades. Pure functions -
 * no state, no allocations, safe to call every frame.
 */

/* Move `current` toward `target` with time constant `tau_ms`.
 * Returns the new value. Snaps when closer than epsilon. */
float anim_approach(float current, float target, float dt_ms, float tau_ms);

/* Smoothstep easing (0..1 in, 0..1 out). */
float anim_ease_smooth(float t);

/* Ease-out cubic. */
float anim_ease_out_cubic(float t);

/* Clamp helper. */
float anim_clamp(float v, float lo, float hi);

/* Ease-out quadratic. */
float anim_ease_out_quad(float t);

/* Triangle wave over a period: ramps 0->1->0 as `age_ms` advances every
 * `period_ms`. `age_ms` may be any unbounded elapsed-time value. */
float anim_pingpong_ms(float age_ms, float period_ms);

#endif /* DISCADIA_ANIMATION_H */
