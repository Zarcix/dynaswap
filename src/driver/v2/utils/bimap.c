#include "common.h"

#include <linux/hashtable.h>
#include <linux/hash.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "utils/bimap.h"

int bimap_insert(bimap *bimap, sector_t sector, unsigned long slot) {
    log_debug("inserting (sector = %llu, slot = %lu)", sector, slot);
    unsigned long flags;

    // Check if the mapping already exists
    struct bimap_entry *existing;

    read_lock_irqsave(&bimap->lock, flags);

    hash_for_each_possible(bimap->sector_to_slot, existing, sector_node, sector) {
        if (existing->sector == sector) {
            read_unlock_irqrestore(&bimap->lock, flags);
            return -EEXIST; 
        }
    }

    read_unlock_irqrestore(&bimap->lock, flags);

    // Now that we know the mapping doesn't exist, create it
    struct bimap_entry *entry = kmem_cache_alloc(bimap->entry_cache, GFP_NOIO);
    if (!entry) return -ENOMEM;

    entry->sector = sector;
    entry->slot = slot;

    write_lock_irqsave(&bimap->lock, flags);

    hash_add(bimap->sector_to_slot, &entry->sector_node, sector);
    hash_add(bimap->slot_to_sector, &entry->slot_node, slot);

    write_unlock_irqrestore(&bimap->lock, flags);

    return 0;
}

int bimap_find_by_slot(bimap *bimap, unsigned long slot, sector_t *sector) {
    struct bimap_entry *entry;
    unsigned long flags;
    int found = -ENOENT;

    read_lock_irqsave(&bimap->lock, flags);

    hash_for_each_possible(bimap->slot_to_sector, entry, slot_node, slot) {
        if (entry->slot == slot) {
            *sector = entry->sector;
            found = 0;
            break;
        }
    }

    read_unlock_irqrestore(&bimap->lock, flags);

    return found;
}

int bimap_find_by_sector(bimap *bimap, sector_t sector, unsigned long *slot) {
    struct bimap_entry *entry;
    unsigned long flags;
    int found = -ENOENT;

    read_lock_irqsave(&bimap->lock, flags);

    hash_for_each_possible(bimap->sector_to_slot, entry, sector_node, sector) {
        if (entry->sector == sector) {
            *slot = entry->slot;
            log_debug("found slot (sector = %llu, slot = %lu)", sector, *slot);
            found = 0;
            break;
        }
    }

    read_unlock_irqrestore(&bimap->lock, flags);

    return found;
}

int init_bimap(bimap *bimap) {
    hash_init(bimap->sector_to_slot);
    hash_init(bimap->slot_to_sector);

    rwlock_init(&bimap->lock);

    bimap->entry_cache = kmem_cache_create("vblk_bimap_entry", sizeof(struct bimap_entry), 0, SLAB_HWCACHE_ALIGN, NULL);
    if (!bimap->entry_cache) return -ENOMEM;

    return 0;
}

void destroy_bimap(bimap *bimap) {
    struct bimap_entry *entry;
    struct hlist_node *tmp;
    int bkt;
    unsigned long flags;

    if (!bimap || !bimap->entry_cache) return;

    write_lock_irqsave(&bimap->lock, flags);

    hash_for_each_safe(bimap->sector_to_slot, bkt, tmp, entry, sector_node) {
        hlist_del(&entry->sector_node);
        hlist_del(&entry->slot_node);
        kmem_cache_free(bimap->entry_cache, entry);
    }

    write_unlock_irqrestore(&bimap->lock, flags);
}