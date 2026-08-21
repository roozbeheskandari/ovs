//add this file by Roozbeh

#include <config.h>
#include "dpif-netdev-ai.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hash.h"
#include "openvswitch/vlog.h"
#include "util.h"

VLOG_DEFINE_THIS_MODULE(dpif_netdev_ai);

/* --- Cuckoo Filter Implementation --- */
static uint16_t get_fingerprint(uint32_t hash) {
    uint16_t fp = hash & 0xFFFF;
    fp += (fp == 0); // Fingerprint cannot be 0 (0 means empty)
    return fp;
}

static size_t hash_to_index(uint32_t hash, size_t num_buckets) {
    return hash % num_buckets;
}

static size_t alt_index(size_t index, uint16_t fp, size_t num_buckets) {
    uint32_t fp_hash = hash_bytes(&fp, sizeof fp, 0);
    return (index ^ fp_hash) % num_buckets;
}
/*
struct cuckoo_filter *cuckoo_filter_create(size_t max_capacity) {
    struct cuckoo_filter *cf = xzalloc(sizeof *cf);
    // Calculate buckets needed, rounding up to nearest power of 2 is preferred, but simple modulo works for now.
    cf->num_buckets = (max_capacity / CUCKOO_BUCKET_SIZE) + 1;
    cf->buckets = xcalloc(cf->num_buckets, sizeof(struct cuckoo_bucket));
    cf->count = 0;
    return cf;
}

void cuckoo_filter_destroy(struct cuckoo_filter *cf) {
    if (cf) {
        free(cf->buckets);
        free(cf);
    }
}

bool cuckoo_filter_insert(struct cuckoo_filter *cf, uint32_t hash) {
    if (!cf) return false;
    uint16_t fp = get_fingerprint(hash);
    size_t i1 = hash_to_index(hash, cf->num_buckets);
    size_t i2 = alt_index(i1, fp, cf->num_buckets);

    // Try to insert in bucket i1
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (cf->buckets[i1].fingerprints[i] == 0) {
            cf->buckets[i1].fingerprints[i] = fp;
            cf->count++;
            return true;
        }
    }
    // Try bucket i2
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (cf->buckets[i2].fingerprints[i] == 0) {
            cf->buckets[i2].fingerprints[i] = fp;
            cf->count++;
            return true;
        }
    }
    
    // Eviction logic (Kicking)
    size_t curr_idx = (random() % 2 == 0) ? i1 : i2;
    for (int n = 0; n < CUCKOO_MAX_KICKS; n++) {
        int rand_slot = random() % CUCKOO_BUCKET_SIZE;
        uint16_t kicked_fp = cf->buckets[curr_idx].fingerprints[rand_slot];
        cf->buckets[curr_idx].fingerprints[rand_slot] = fp;
        
        fp = kicked_fp;
        curr_idx = alt_index(curr_idx, fp, cf->num_buckets);
        for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
            if (cf->buckets[curr_idx].fingerprints[i] == 0) {
                cf->buckets[curr_idx].fingerprints[i] = fp;
                cf->count++;
                return true;
            }
        }
    }
    return false; // Filter is full
}

bool cuckoo_filter_lookup(struct cuckoo_filter *cf, uint32_t hash) {
    if (!cf) return true; // Fail open if no filter
    uint16_t fp = get_fingerprint(hash);
    size_t i1 = hash_to_index(hash, cf->num_buckets);
    size_t i2 = alt_index(i1, fp, cf->num_buckets);

    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (cf->buckets[i1].fingerprints[i] == fp || 
            cf->buckets[i2].fingerprints[i] == fp) {
            return true;
        }
    }
    return false;
}*/

/* Wrappers used by dpif-netdev.c and dpif-netdev-dpcls.c */
bool global_cuckoo_filter_lookup(void *filter, uint32_t hash) {
    return cuckoo_filter_lookup((struct cuckoo_filter *)filter, hash);
}

bool per_subtable_cuckoo_filter_lookup(void *filter, uint32_t hash) {
    return cuckoo_filter_lookup((struct cuckoo_filter *)filter, hash);
}

/* --- Learned Gate AI Model Implementation --- */
struct learned_gate_model *learned_gate_model_create(void) {
    struct learned_gate_model *model = xzalloc(sizeof *model);
    // Initialize with default weights (these should be updated by control plane)
    for (int i = 0; i < MAX_FEATURES; i++) {
        model->weights[i] = 1.0f; 
    }
    model->bias = 0.0f;
    return model;
}

void learned_gate_model_destroy(struct learned_gate_model *model) {
    free(model);
}

float calculate_learned_score(struct learned_gate_model *model, const struct netdev_flow_key *key) {
    if (!model || !key) return 0.0f;
    
    // استخراج ویژگی‌ها از بسته (مثلاً hash, len, TCP/UDP ports و ...)
    // برای سادگی در اینجا از hash خود کلید به عنوان ویژگی استفاده شده است.
    // در مقاله شما باید ویژگی‌های واقعی (Features) را استخراج کنید.
    float score = model->bias;
    float feature_val = (float)(key->hash % 100) / 100.0f; 
    
    score += model->weights[0] * feature_val;
    
    // تابع Sigmoid برای خروجی احتمالی بین 0 و 1
    return 1.0f / (1.0f + expf(-score));
}
