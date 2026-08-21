    #ifndef CUCKOO_FILTER_H
    #define CUCKOO_FILTER_H

    #include <stdbool.h>
    #include <stddef.h>
    #include <stdint.h>

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

    struct cuckoo_filter *cuckoo_filter_create(size_t capacity);
    void cuckoo_filter_destroy(struct cuckoo_filter *cf);
    bool cuckoo_filter_insert(struct cuckoo_filter *cf, uint32_t hash);
    bool cuckoo_filter_lookup(const struct cuckoo_filter *cf, uint32_t hash);

    #endif /* CUCKOO_FILTER_H */
