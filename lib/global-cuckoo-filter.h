/* Copyright (c) 2026.
 * Licensed under the Apache License, Version 2.0. */

#ifndef GLOBAL_CUCKOO_FILTER_H
#define GLOBAL_CUCKOO_FILTER_H 1

#include <stdbool.h>

#include "dpif-netdev-dpcls.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Acquires the process-wide global Cuckoo filter.
 *
 * One reference is taken for each initialized dpcls classifier.  The filter
 * is released when the corresponding classifier is destroyed.
 */
void global_cuckoo_filter_init(void);

/* Releases one reference acquired by global_cuckoo_filter_init(). */
void global_cuckoo_filter_destroy(void);

/* Adds/removes a flow key fingerprint to/from the global filter.
 *
 * Insert/remove are advisory index maintenance.  A failed insertion or an
 * imprecise removal must never affect datapath correctness because dpcls is
 * always used as the final classifier.
 */
bool global_cuckoo_filter_insert(const struct netdev_flow_key *key);
void global_cuckoo_filter_remove(const struct netdev_flow_key *key);

/* Returns whether KEY may be represented in the global Cuckoo filter.
 *
 * A false result is not a final miss.  It may only be used for statistics,
 * policy selection, prefetching, or future candidate ordering.
 */
bool global_cuckoo_filter_lookup(const struct netdev_flow_key *key);

/* Records the joint result of AI policy scoring and global-filter probing.
 * This is intentionally separate from lookup so that the policy layer stays
 * independent of the filter implementation. */
void global_cuckoo_filter_note_policy(bool ai_candidate, bool filter_hit);

#ifdef __cplusplus
}
#endif

#endif /* global-cuckoo-filter.h */
