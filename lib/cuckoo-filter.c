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
        return fp ? fp : 1;
    }

    static inline void get_index(uint32_t hash, uint16_t fp, size_t num_buckets, size_t *i1, size_t *i2) {
        *i1 = (hash) % num_buckets;
        *i2 = ((*i1) ^ (fp * 0x5bd1e995)) % num_buckets;
    }
/*
    struct cuckoo_filter *cuckoo_filter_create(size_t capacity) {
        size_t num_buckets = capacity / CUCKOO_BUCKET_SIZE;
        if (num_buckets == 0) num_buckets = 1;
        struct cuckoo_filter *cf = xzalloc(sizeof *cf);
        cf->buckets = xzalloc(num_buckets * sizeof *cf->buckets);
        cf->num_buckets = num_buckets;
        cf->count = 0;
        return cf;
    }
*/
struct cuckoo_filter *
cuckoo_filter_create(size_t capacity)
{
    size_t num_buckets = capacity / CUCKOO_BUCKET_SIZE;

    if (capacity % CUCKOO_BUCKET_SIZE) {
        num_buckets++;
    }

    if (num_buckets == 0) {
        num_buckets = 1;
    }
 	struct cuckoo_filter *cf = xzalloc(sizeof *cf);
    if (num_buckets > SIZE_MAX / sizeof *cf->buckets) {
        VLOG_ERR("Cuckoo filter allocation overflow: capacity=%zu, "
                 "num_buckets=%zu",
                 capacity, num_buckets);
                 free(cf);
        return NULL;
    }

   

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

    /* ۱. تلاش برای درج در باکت اول */
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (cf->buckets[i1].fingerprints[i] == 0) {
            cf->buckets[i1].fingerprints[i] = fp;
            cf->count++;
            return true;
        }
    }

    /* ۲. تلاش برای درج در باکت دوم */
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (cf->buckets[i2].fingerprints[i] == 0) {
            cf->buckets[i2].fingerprints[i] = fp;
            cf->count++;
            return true;
        }
    }

    /* ۳. در صورت پر بودن هر دو باکت، شروع فرآیند Kickout با ثبت تاریخچه برای Rollback */
    struct kick_entry kick_log[CUCKOO_MAX_KICKS];
    int kicks_done = 0;

    size_t cur_i = (random_uint32() % 2 == 0) ? i1 : i2;
    for (int kick = 0; kick < CUCKOO_MAX_KICKS; kick++) {
        int slot = random_uint32() % CUCKOO_BUCKET_SIZE;
        uint16_t temp = cf->buckets[cur_i].fingerprints[slot];

        /* ثبت وضعیت قبلی برای بازگردانی در صورت شکست */
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

    /* ۴. اگر به سقف کیک‌ها رسید و جا پیدا نشد: برگرداندن تغییرات (Rollback) تا دیتای قبلی فیلتر خراب نشود */
    for (int i = kicks_done - 1; i >= 0; i--) {
        cf->buckets[kick_log[i].bucket_idx].fingerprints[kick_log[i].slot] = kick_log[i].old_fp;
    }

    return false;
}


    bool cuckoo_filter_lookup(const struct cuckoo_filter *cf, uint32_t hash) {
        if (!cf || cf->count == 0) return false;
        uint16_t fp = get_fingerprint(hash);
        size_t i1, i2;
        get_index(hash, fp, cf->num_buckets, &i1, &i2);

        for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
            if (cf->buckets[i1].fingerprints[i] == fp) return true;
            if (cf->buckets[i2].fingerprints[i] == fp) return true;
        }
        return false;
    }
