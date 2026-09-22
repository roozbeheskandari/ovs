/* Copyright (c) 2026.
 * Licensed under the Apache License, Version 2.0. */

#ifndef DPIF_NETDEV_AI_H
#define DPIF_NETDEV_AI_H 1

#include <stdbool.h>
#include <stdint.h>

#include "dpif-netdev-dpcls.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Result of the AI policy scorer.
 *
 * probability is in the inclusive range [0, 1000].  It represents the
 * logistic-regression estimate that the key is worth consulting in the
 * global-filter candidate path.
 *
 * This result is advisory only.  It must never be used to suppress the
 * mandatory dpcls lookup, because that could introduce false negatives.
 */
struct dp_netdev_ai_result {
    uint16_t probability;
    bool candidate;
};

/* Computes an advisory score for KEY.
 *
 * This function has no mutable state and is safe to call from PMD threads.
 * The current model uses inexpensive features derived from the flow-key hash
 * and miniflow length.  Model coefficients can later be replaced with
 * coefficients learned offline, without changing the datapath interface.
 */
struct dp_netdev_ai_result
dp_netdev_ai_score(const struct netdev_flow_key *key);

#ifdef __cplusplus
}
#endif

#endif /* dpif-netdev-ai.h */
