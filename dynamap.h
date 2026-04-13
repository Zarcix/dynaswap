struct dynaswap_ctx;

// Allocation & Metadata
static unsigned long dynaswap_get_slot(struct dynaswap_ctx *ctx);
static void dynaswap_put_slot(struct dynaswap_ctx *ctx, unsigned long slot);

// IO Operations
void dynaswap_write(struct dynaswap_ctx *ctx, sector_t sector, struct page *page);
void dynaswap_read(struct dynaswap_ctx *ctx, sector_t sector, struct page *page);
void dynaswap_discard(struct dynaswap_ctx *ctx, sector_t sector, unsigned int nr_sectors);

// Sizing Logic
void dynaswap_shrink(struct dynaswap_ctx *ctx); // The "Two-Pointer" Compaction logic
static int dynaswap_move_block(struct dynaswap_ctx *ctx, unsigned long old_slot, unsigned long new_slot);

// Setup/Teardown
int dynamap_init(struct dynaswap_ctx *ctx, const char *path);
void dynamap_cleanup(struct dynaswap_ctx *ctx);