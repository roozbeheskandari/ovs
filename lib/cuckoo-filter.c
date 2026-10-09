#include <config.h>
#include <stdint.h>
#include <stdlib.h>

#include "cuckoo-filter.h"
#include "util.h"
#include "random.h"
#include "openvswitch/vlog.h"

VLOG_DEFINE_THIS_MODULE(cuckoo_filter);

static inline uint16_t get_fingerprint(uint32_t hash) {
    uint16_t fp = hash & 0xFFFF;
    return fp ? fp : 1; /* اثرانگشت صفر به عنوان اسلات خالی رزرو است */
}

static inline void get_index(uint32_t hash, uint16_t fp, size_t num_buckets, size_t *i1, size_t *i2) {
    *i1 = hash % num_buckets;
    *i2 = ((*i1) ^ (fp * 0x5bd1e995)) % num_buckets;
}

struct cuckoo_filter *cuckoo_filter_create(size_t capacity) {
    size_t num_buckets = capacity / CUCKOO_BUCKET_SIZE;
    if (num_buckets == 0) {
        num_buckets = 1;
    }
    struct cuckoo_filter *cf = xzalloc(sizeof *cf);
    cf->buckets = xzalloc(num_buckets * sizeof *cf->buckets);
    cf->num_buckets = num_buckets;
    cf->count = 0;
    return cf;
}

void cuckoo_filter_destroy(struct cuckoo_filter *cf) {
    if (cf) {
        free(cf->buckets);
        free(cf);
    }
}

struct kick_entry {
    size_t bucket_idx;
    int slot;
    uint16_t old_fp;
};

bool cuckoo_filter_insert(struct cuckoo_filter *cf, uint32_t hash) {
    uint16_t fp = get_fingerprint(hash);
    size_t i1, i2;
    get_index(hash, fp, cf->num_buckets, &i1, &i2);

    /* تلاش برای باکت اول */
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (cf->buckets[i1].fingerprints[i] == 0) {
            cf->buckets[i1].fingerprints[i] = fp;
            cf->count++;
            return true;
        }
    }

    /* تلاش برای باکت دوم */
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (cf->buckets[i2].fingerprints[i] == 0) {
            cf->buckets[i2].fingerprints[i] = fp;
            cf->count++;
            return true;
        }
    }

    /* شروع Kickout با قابلیت Rollback کامل */
    struct kick_entry kick_log[CUCKOO_MAX_KICKS];
    int kicks_done = 0;

    size_t cur_i = (random_uint32() % 2 == 0) ? i1 : i2;
    for (int kick = 0; kick < CUCKOO_MAX_KICKS; kick++) {
        int slot = random_uint32() % CUCKOO_BUCKET_SIZE;
        uint16_t temp = cf->buckets[cur_i].fingerprints[slot];

        kick_log[kicks_done].bucket_idx = cur_i;
        kick_log[kicks_done].slot = slot;
        kick_log[kicks_done].old_fp = temp;
        kicks_done++;

        cf->buckets[cur_i].fingerprints[slot] = fp;
        fp = temp;

        cur_i = (cur_i ^ (fp * 0x5bd1e995)) % cf->num_buckets;
        for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
            if (cf->buckets[cur_i].fingerprints[i] == 0) {
                cf->buckets[cur_i].fingerprints[i] = fp;
                cf->count++;
                return true;
            }
        }
    }

    /* Rollback در صورت اشباع */
    for (int i = kicks_done - 1; i >= 0; i--) {
        cf->buckets[kick_log[i].bucket_idx].fingerprints[kick_log[i].slot] = kick_log[i].old_fp;
    }

    return false;
}

bool cuckoo_filter_lookup(const struct cuckoo_filter *cf, uint32_t hash) {
    if (!cf || cf->num_buckets == 0) {
        return false;
    }
    uint16_t fp = get_fingerprint(hash);
    size_t i1, i2;
    get_index(hash, fp, cf->num_buckets, &i1, &i2);

    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (cf->buckets[i1].fingerprints[i] == fp) {
            return true;
        }
    }
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (cf->buckets[i2].fingerprints[i] == fp) {
            return true;
        }
    }
    return false;
}
