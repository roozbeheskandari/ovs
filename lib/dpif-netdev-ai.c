/* Copyright (c) 2026.
 * Licensed under the Apache License, Version 2.0. */

#include <config.h>

#include "dpif-netdev-ai.h"

#include <stdint.h>

/* Return a sigmoid-like probability in [0, 1000] without libm.
 *
 * The rational approximation avoids exp(), floating-point instructions and
 * a new libm link dependency in the PMD fast path:
 *
 *              500 * z
 * p(z) = 500 + ---------, for z >= 0
 *             z + 1024
 *
 * and its symmetric counterpart for negative z.
 */
static uint16_t
dp_netdev_ai_sigmoid_q10(int32_t z)
{
    uint32_t magnitude;

    if (z >= 4096) {
        return 1000;
    }
    if (z <= -4096) {
        return 0;
    }

    if (z >= 0) {
        return 500 + (uint16_t) ((500 * (uint32_t) z)
                                 / ((uint32_t) z + 1024));
    }

    magnitude = (uint32_t) -z;
    return 500 - (uint16_t) ((500 * magnitude) / (magnitude + 1024));
}

/* Mix a 32-bit value sufficiently for the compact feature extraction below.
 * This is not used as an OVS flow hash; KEY->hash remains the authoritative
 * dpcls hash. */
static uint32_t
dp_netdev_ai_mix32(uint32_t value)
{
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;

    return value;
}

struct dp_netdev_ai_result
dp_netdev_ai_score(const struct netdev_flow_key *key)
{
    struct dp_netdev_ai_result result;
    uint32_t mixed_hash;
    uint32_t hash_feature;
    uint32_t length_feature;
    int32_t z;

    /* Keep the model deliberately small: all inputs are already available
     * after flow-key extraction, so no packet parsing is added to the PMD
     * fast path. */
    mixed_hash = dp_netdev_ai_mix32(key->hash);
    hash_feature = mixed_hash & UINT32_C(0x3ff);
    length_feature = key->len > 255 ? 255 : key->len;

    /* Logistic-regression model in Q10-like integer form.
     *
     * Features:
     *   - normalized hash fragment: spreads traffic across the model domain.
     *   - miniflow length: approximates key complexity.
     *   - hash/length interaction: separates short and long flow keys.
     *
     * These default coefficients are conservative bootstrap coefficients.
     * Replace them with offline-trained coefficients for an experiment, while
     * preserving the same feature definitions and score interface.
     */
    z = -192;
    z += (int32_t) hash_feature - 512;
    z += ((int32_t) length_feature - 32) * 6;
    z += (int32_t) ((mixed_hash >> 20) & 0x3f) * 4;

    result.probability = dp_netdev_ai_sigmoid_q10(z);

    /* candidate is advisory.  Callers must still execute dpcls_lookup(). */
    result.candidate = result.probability >= 500;

    return result;
}
