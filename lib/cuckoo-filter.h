//Add thid file by Roozbeh Eskandari
#ifndef CUCKOO_FILTER_H
#define CUCKOO_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct cuckoo_filter {
    uint8_t *buckets;
    size_t num_buckets;
};

/* توابع مدیریت فیلتر */
struct cuckoo_filter *cuckoo_filter_create(size_t capacity);
void cuckoo_filter_destroy(struct cuckoo_filter *cf);

/* توابع عملیاتی */
bool cuckoo_filter_insert(struct cuckoo_filter *cf, uint32_t hash);
bool cuckoo_filter_lookup(struct cuckoo_filter *cf, uint32_t hash);

/* توابع پوششی برای اتصال به dpcls */
bool global_cuckoo_filter_lookup(void *filter, uint32_t hash);
bool per_subtable_cuckoo_filter_lookup(void *filter, uint32_t hash);

#endif /* CUCKOO_FILTER_H */
