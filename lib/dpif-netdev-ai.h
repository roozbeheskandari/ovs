//add this file by Roozbeh

#ifndef DPIF_NETDEV_AI_H
#define DPIF_NETDEV_AI_H 1

#include <stdint.h>
#include <stdbool.h>
#include "dpif-netdev.h"

/* --- Cuckoo Filter Definitions --- */
/* یک پیاده‌سازی ساده برای Cuckoo Filter. برای مقالات سطح بالا (Q1) 
 * بهتر است از کتابخانه‌های بهینه SIMD استفاده کنید، اما این ساختار پایه است. */

/*#define CUCKOO_BUCKET_SIZE 4
#define CUCKOO_MAX_KICKS 500

struct cuckoo_bucket {
    uint16_t fingerprints[CUCKOO_BUCKET_SIZE];
};

struct cuckoo_filter {
    struct cuckoo_bucket *buckets;
    size_t num_buckets;
    size_t count;
};
*/
/* توابع Cuckoo Filter */
struct cuckoo_filter *cuckoo_filter_create(size_t max_capacity);
void cuckoo_filter_destroy(struct cuckoo_filter *cf);
bool cuckoo_filter_insert(struct cuckoo_filter *cf, uint32_t hash);
bool cuckoo_filter_lookup(struct cuckoo_filter *cf, uint32_t hash);

/* توابع Wrapper برای OVS */
bool global_cuckoo_filter_lookup(void *filter, uint32_t hash);
bool per_subtable_cuckoo_filter_lookup(void *filter, uint32_t hash);

/* --- Learned Gate Definitions --- */
#define MAX_FEATURES 10

struct learned_gate_model {
    float weights[MAX_FEATURES];
    float bias;
};

struct learned_gate_model *learned_gate_model_create(void);
void learned_gate_model_destroy(struct learned_gate_model *model);
float calculate_learned_score(struct learned_gate_model *model, const struct netdev_flow_key *key);

#endif /* DPIF_NETDEV_AI_H */
