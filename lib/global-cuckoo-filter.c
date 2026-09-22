/* Copyright (c) 2026.
 * Licensed under the Apache License, Version 2.0. */

#include <config.h>

#include "global-cuckoo-filter.h"

#include <stdint.h>

#include "openvswitch/thread.h"
#include "util.h"

/* Keep the table size a power of two, since alternate bucket computation
 * uses an XOR operation. */
#define GLOBAL_CUCKOO_BUCKET_COUNT  (1u << 16)
#define GLOBAL_CUCKOO_BUCKET_MASK   (GLOBAL_CUCKOO_BUCKET_COUNT - 1)
#define GLOBAL_CUCKOO_BUCKET_SIZE   4
#define GLOBAL_CUCKOO_MAX_KICKS     500

struct global_cuckoo_bucket {
    uint16_t fingerprints[GLOBAL_CUCKOO_BUCKET_SIZE];
};

struct global_cuckoo_filter {
    struct global_cuckoo_bucket *buckets;
    uint32_t users;
    uint32_t random_state;

    uint64_t inserts;
    uint64_t insert_failures;
    uint64_t removes;
    uint64_t lookups;
    uint64_t filter_hits;
    uint64_t ai_candidates;
    uint64_t ai_filter_hits;
};

static struct global_cuckoo_filter *global_filter;
static struct ovs_mutex global_filter_mutex = OVS_MUTEX_INITIALIZER;

static uint32_t
global_cuckoo_mix32(uint32_t value)
{
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;

    return value;
}

static uint16_t
global_cuckoo_fingerprint(const struct netdev_flow_key *key)
{
    uint16_t fingerprint;

    fingerprint = (uint16_t) global_cuckoo_mix32(key->hash ^ key->len);
    return fingerprint ? fingerprint : 1;
}

static uint32_t
global_cuckoo_index1(const struct netdev_flow_key *key)
{
    return global_cuckoo_mix32(key->hash) & GLOBAL_CUCKOO_BUCKET_MASK;
}

static uint32_t
global_cuckoo_index2(uint32_t index1, uint16_t fingerprint)
{
    return (index1 ^ global_cuckoo_mix32(fingerprint))
           & GLOBAL_CUCKOO_BUCKET_MASK;
}

static bool
global_cuckoo_bucket_contains(const struct global_cuckoo_bucket *bucket,
                              uint16_t fingerprint)
{
    size_t i;

    for (i = 0; i < GLOBAL_CUCKOO_BUCKET_SIZE; i++) {
        if (bucket->fingerprints[i] == fingerprint) {
            return true;
        }
    }

    return false;
}

static bool
global_cuckoo_bucket_insert(struct global_cuckoo_bucket *bucket,
                            uint16_t fingerprint)
{
    size_t i;

    for (i = 0; i < GLOBAL_CUCKOO_BUCKET_SIZE; i++) {
        if (!bucket->fingerprints[i]) {
            bucket->fingerprints[i] = fingerprint;
            return true;
        }
    }

    return false;
}

static bool
global_cuckoo_bucket_remove(struct global_cuckoo_bucket *bucket,
                            uint16_t fingerprint)
{
    size_t i;

    for (i = 0; i < GLOBAL_CUCKOO_BUCKET_SIZE; i++) {
        if (bucket->fingerprints[i] == fingerprint) {
            bucket->fingerprints[i] = 0;
            return true;
        }
    }

    return false;
}

static uint32_t
global_cuckoo_next_random(struct global_cuckoo_filter *filter)
{
    uint32_t value = filter->random_state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;

    filter->random_state = value;
    return value;
}

void
global_cuckoo_filter_init(void)
{
    ovs_mutex_lock(&global_filter_mutex);

    if (!global_filter) {
        global_filter = xzalloc(sizeof *global_filter);
        global_filter->buckets = xzalloc(GLOBAL_CUCKOO_BUCKET_COUNT
                                         * sizeof *global_filter->buckets);
        global_filter->random_state = UINT32_C(0x6d2b79f5);
    }

    global_filter->users++;

    ovs_mutex_unlock(&global_filter_mutex);
}

void
global_cuckoo_filter_destroy(void)
{
    ovs_mutex_lock(&global_filter_mutex);

    ovs_assert(global_filter != NULL);
    ovs_assert(global_filter->users > 0);

    global_filter->users--;

    if (!global_filter->users) {
        free(global_filter->buckets);
        free(global_filter);
        global_filter = NULL;
    }

    ovs_mutex_unlock(&global_filter_mutex);
}

bool
global_cuckoo_filter_insert(const struct netdev_flow_key *key)
{
    struct global_cuckoo_filter *filter;
    struct global_cuckoo_bucket *bucket;
    uint16_t fingerprint;
    uint16_t evicted;
    uint32_t index1;
    uint32_t index2;
    uint32_t index;
    size_t i;

    ovs_mutex_lock(&global_filter_mutex);

    filter = global_filter;
    if (!filter) {
        ovs_mutex_unlock(&global_filter_mutex);
        return false;
    }

    fingerprint = global_cuckoo_fingerprint(key);
    index1 = global_cuckoo_index1(key);
    index2 = global_cuckoo_index2(index1, fingerprint);

    if (global_cuckoo_bucket_contains(&filter->buckets[index1],
                                      fingerprint)
        || global_cuckoo_bucket_contains(&filter->buckets[index2],
                                         fingerprint)) {
        ovs_mutex_unlock(&global_filter_mutex);
        return true;
    }

    if (global_cuckoo_bucket_insert(&filter->buckets[index1], fingerprint)
        || global_cuckoo_bucket_insert(&filter->buckets[index2],
                                       fingerprint)) {
        filter->inserts++;
        ovs_mutex_unlock(&global_filter_mutex);
        return true;
    }

    index = (global_cuckoo_next_random(filter) & 1) ? index1 : index2;

    for (i = 0; i < GLOBAL_CUCKOO_MAX_KICKS; i++) {
        bucket = &filter->buckets[index];
        evicted = bucket->fingerprints[
            global_cuckoo_next_random(filter) % GLOBAL_CUCKOO_BUCKET_SIZE];
        bucket->fingerprints[
            global_cuckoo_next_random(filter) % GLOBAL_CUCKOO_BUCKET_SIZE]
            = fingerprint;

        fingerprint = evicted;
        index = global_cuckoo_index2(index, fingerprint);

        if (global_cuckoo_bucket_insert(&filter->buckets[index],
                                        fingerprint)) {
            filter->inserts++;
            ovs_mutex_unlock(&global_filter_mutex);
            return true;
        }
    }

    filter->insert_failures++;
    ovs_mutex_unlock(&global_filter_mutex);

    return false;
}

void
global_cuckoo_filter_remove(const struct netdev_flow_key *key)
{
    struct global_cuckoo_filter *filter;
    uint16_t fingerprint;
    uint32_t index1;
    uint32_t index2;

    ovs_mutex_lock(&global_filter_mutex);

    filter = global_filter;
    if (!filter) {
        ovs_mutex_unlock(&global_filter_mutex);
        return;
    }

    fingerprint = global_cuckoo_fingerprint(key);
    index1 = global_cuckoo_index1(key);
    index2 = global_cuckoo_index2(index1, fingerprint);

    if (global_cuckoo_bucket_remove(&filter->buckets[index1], fingerprint)
        || global_cuckoo_bucket_remove(&filter->buckets[index2],
                                       fingerprint)) {
        filter->removes++;
    }

    ovs_mutex_unlock(&global_filter_mutex);
}

bool
global_cuckoo_filter_lookup(const struct netdev_flow_key *key)
{
    struct global_cuckoo_filter *filter;
    uint16_t fingerprint;
    uint32_t index1;
    uint32_t index2;
    bool hit = false;

    ovs_mutex_lock(&global_filter_mutex);

    filter = global_filter;
    if (filter) {
        fingerprint = global_cuckoo_fingerprint(key);
        index1 = global_cuckoo_index1(key);
        index2 = global_cuckoo_index2(index1, fingerprint);

        hit = global_cuckoo_bucket_contains(&filter->buckets[index1],
                                            fingerprint)
              || global_cuckoo_bucket_contains(&filter->buckets[index2],
                                               fingerprint);

        filter->lookups++;
        if (hit) {
            filter->filter_hits++;
        }
    }

    ovs_mutex_unlock(&global_filter_mutex);

    return hit;
}

void
global_cuckoo_filter_note_policy(bool ai_candidate, bool filter_hit)
{
    ovs_mutex_lock(&global_filter_mutex);

    if (global_filter && ai_candidate) {
        global_filter->ai_candidates++;
        if (filter_hit) {
            global_filter->ai_filter_hits++;
        }
    }

    ovs_mutex_unlock(&global_filter_mutex);
}
