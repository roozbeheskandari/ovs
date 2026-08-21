//Add thid file by Roozbeh Eskandari
#ifndef CUCKOO_FILTER_H
#define CUCKOO_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define CUCKOO_BUCKET_SIZE 4
#define CUCKOO_MAX_KICKS 500

struct cuckoo_bucket {
    uint16_t fingerprints[CUCKOO_BUCKET_SIZE];
};

struct cuckoo_filter {
    struct cuckoo_bucket *buckets;
    size_t num_buckets;
    size_t count;
};

/* توابع مدیریت فیلتر */
struct cuckoo_filter *cuckoo_filter_create(size_t capacity);
void cuckoo_filter_destroy(struct cuckoo_filter *cf);

/* توابع عملیاتی */
bool cuckoo_filter_insert(struct cuckoo_filter *cf, uint32_t hash);
bool cuckoo_filter_lookup(struct cuckoo_filter *cf, uint32_t hash);

/* توابع پوششی برای اتصال به dpcls */
//bool global_cuckoo_filter_lookup(void *filter, uint32_t hash);
//bool per_subtable_cuckoo_filter_lookup(void *filter, uint32_t hash);

#endif /* CUCKOO_FILTER_H */
