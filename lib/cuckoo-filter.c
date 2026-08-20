//Add this file by Roozbeh Eskandari

#include "cuckoo-filter.h"
#include <string.h>

#define BUCKET_SIZE 4

struct cuckoo_filter *
cuckoo_filter_create(size_t capacity)
{
    struct cuckoo_filter *cf = malloc(sizeof *cf);
    if (!cf) return NULL;
    
    // ساده‌سازی: ایجاد آرایه برای نگهداری اثر انگشت‌ها (Fingerprints)
    cf->num_buckets = capacity > 0 ? capacity : 1024; 
    cf->buckets = calloc(cf->num_buckets, BUCKET_SIZE);
    return cf;
}

void
cuckoo_filter_destroy(struct cuckoo_filter *cf)
{
    if (cf) {
        free(cf->buckets);
        free(cf);
    }
}

// برای سادگی در این دمو، یک فیلتر بلوم/هش مپ ساده جایگزین منطق پیچیده جابجایی کوکو شده است
bool
cuckoo_filter_insert(struct cuckoo_filter *cf, uint32_t hash)
{
    if (!cf) return false;
    uint32_t index = hash % cf->num_buckets;
    uint8_t fingerprint = (hash & 0xFF) == 0 ? 1 : (hash & 0xFF); // جلوگیری از 0
    
    for (int i = 0; i < BUCKET_SIZE; i++) {
        if (cf->buckets[index * BUCKET_SIZE + i] == 0) {
            cf->buckets[index * BUCKET_SIZE + i] = fingerprint;
            return true;
        }
    }
    return false; // سرریز
}

bool
cuckoo_filter_lookup(struct cuckoo_filter *cf, uint32_t hash)
{
    if (!cf) return false;
    uint32_t index = hash % cf->num_buckets;
    uint8_t fingerprint = (hash & 0xFF) == 0 ? 1 : (hash & 0xFF);
    
    for (int i = 0; i < BUCKET_SIZE; i++) {
        if (cf->buckets[index * BUCKET_SIZE + i] == fingerprint) {
            return true;
        }
    }
    return false;
}

bool global_cuckoo_filter_lookup(void *filter, uint32_t hash) {
    return cuckoo_filter_lookup((struct cuckoo_filter *)filter, hash);
}

bool per_subtable_cuckoo_filter_lookup(void *filter, uint32_t hash) {
    return cuckoo_filter_lookup((struct cuckoo_filter *)filter, hash);
}
