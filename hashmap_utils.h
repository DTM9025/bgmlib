// Music Room BGM Library
// ----------------------
// hashmap_utils.h - Utility structs and functions for hashmap
// ----------------------
// "©" DTM9025, 2024

#ifndef BGMLIB_HASHMAP_UTILS_H
#define BGMLIB_HASHMAP_UTILS_H

#include "infostruct.h"

struct DictItem {
	uint32_t hash;
	struct TrackInfo* TI;
};

static int dict_compare(const void *a, const void *b, void *udata) {
	uint32_t ha = ((DictItem*)a)->hash;
	uint32_t hb = ((DictItem*)b)->hash;

	if (ha < hb) return -1; // Need to do this to avoid underflow
	if (ha > hb) return 1;
	return 0;
}

static uint64_t dict_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct DictItem *entry = (DictItem*)item;
    return hashmap_sip(&entry->hash, sizeof(uint32_t), seed0, seed1);
}

#endif /* BGMLIB_HASHMAP_UTILS_H */
