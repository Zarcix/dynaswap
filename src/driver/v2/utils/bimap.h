#ifndef UTILS_BIMAP
#define UTILS_BIMAP

#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/spinlock_types.h>

#define BIMAP_BITS 10

struct bimap_entry {
    sector_t sector;
    unsigned long slot;

    struct hlist_node sector_node;
    struct hlist_node slot_node;
};

typedef struct {
    DECLARE_HASHTABLE(sector_to_slot, BIMAP_BITS);
    DECLARE_HASHTABLE(slot_to_sector, BIMAP_BITS);
    struct kmem_cache *entry_cache;
    rwlock_t lock;
} bimap;

int bimap_insert(bimap *bimap, sector_t sector, unsigned long slot);

// int bimap_remove_by_slot(bimap *bimap, unsigned long slot);
// int bimap_remove_by_sector(bimap *bimap, sector_t sector);

int bimap_find_by_slot(bimap *bimap, unsigned long slot, sector_t *sector);
int bimap_find_by_sector(bimap *bimap, sector_t sector, unsigned long *slot);

int init_bimap(bimap *bimap);
void destroy_bimap(bimap *bimap);

#endif