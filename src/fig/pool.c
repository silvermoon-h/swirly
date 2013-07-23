/*
 *  Copyright (C) 2013 Mark Aylett <mark.aylett@gmail.com>
 *
 *  This file is part of Doobry written by Mark Aylett.
 *
 *  Doobry is free software; you can redistribute it and/or modify it under the terms of the GNU
 *  General Public License as published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Doobry is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 *  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program; if
 *  not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301 USA.
 */
#include "pool.h"

#include <dbr/conv.h>
#include <dbr/err.h>
#include <dbr/pool.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static DbrBool
alloc_small_nodes(struct FigPool* pool)
{
    struct FigSmallBlock* block = malloc(sizeof(struct FigSmallBlock)
                                         + pool->small.nodes_per_block
                                         * sizeof(struct FigSmallNode));
    if (dbr_unlikely(!block)) {
        dbr_err_set(DBR_ENOMEM, "out of memory");
        return false;
    }

    // Push block.
    block->next = pool->small.first_block;
    pool->small.first_block = block;

    // Link chain of new nodes.
    const int last = pool->small.nodes_per_block - 1;
    for (int i = 0; i < last; ++i) {
        block->nodes[i].next = &block->nodes[i + 1];
#if defined(DBR_DEBUG_ALLOC)
        block->nodes[i].file = NULL;
        block->nodes[i].line = 0;
#endif // DBR_DEBUG_ALLOC
    }

    // Last node links to any existing nodes.
    block->nodes[last].next = pool->small.first_node;
#if defined(DBR_DEBUG_ALLOC)
    block->nodes[last].file = NULL;
    block->nodes[last].line = 0;
#endif // DBR_DEBUG_ALLOC

    // Newly allocated nodes are now at the front.
    pool->small.first_node = &block->nodes[0];

    return true;
}

static DbrBool
alloc_large_nodes(struct FigPool* pool)
{
    struct FigLargeBlock* block = malloc(sizeof(struct FigLargeBlock)
                                         + pool->large.nodes_per_block
                                         * sizeof(struct FigLargeNode));
    if (dbr_unlikely(!block)) {
        dbr_err_set(DBR_ENOMEM, "out of memory");
        return false;
    }

    // Push block.
    block->next = pool->large.first_block;
    pool->large.first_block = block;

    // Link chain of new nodes.
    const int last = pool->large.nodes_per_block - 1;
    for (int i = 0; i < last; ++i) {
        block->nodes[i].next = &block->nodes[i + 1];
#if defined(DBR_DEBUG_ALLOC)
        block->nodes[i].file = NULL;
        block->nodes[i].line = 0;
#endif // DBR_DEBUG_ALLOC
    }

    // Last node links to any existing nodes.
    block->nodes[last].next = pool->large.first_node;
#if defined(DBR_DEBUG_ALLOC)
    block->nodes[last].file = NULL;
    block->nodes[last].line = 0;
#endif // DBR_DEBUG_ALLOC

    // Newly allocated nodes are now at the front.
    pool->large.first_node = &block->nodes[0];

    return true;
}

DBR_EXTERN DbrBool
fig_pool_init(struct FigPool* pool)
{
    // Slightly less than one page of items.
    // ((page_size - header_size) / item_size) - 1

    const long page_size = sysconf(_SC_PAGESIZE);

    // Small.
    pool->small.nodes_per_block = (page_size - sizeof(struct FigSmallBlock))
        / sizeof(struct FigSmallNode) - 1;
    pool->small.first_block = NULL;
    pool->small.first_node = NULL;

    // Large.
    pool->large.nodes_per_block = (page_size - sizeof(struct FigLargeBlock))
        / sizeof(struct FigLargeNode) - 1;
    pool->large.first_block = NULL;
    pool->large.first_node = NULL;

#if defined(DBR_DEBUG_ALLOC)
    pool->allocs = 0;
    pool->checksum = 0;
#if DBR_DEBUG_LEVEL >= 1
    fprintf(stderr, "%zu small nodes per block:\n", pool->small.nodes_per_block);
    fprintf(stderr, "sizeof DbrLevel=%zu\n", sizeof(struct DbrLevel));
    fprintf(stderr, "sizeof DbrMatch=%zu\n", sizeof(struct DbrMatch));
    fprintf(stderr, "sizeof DbrMemb=%zu\n", sizeof(struct DbrMemb));
    fprintf(stderr, "sizeof DbrPosn=%zu\n", sizeof(struct DbrPosn));
    fprintf(stderr, "sizeof DbrSub=%zu\n", sizeof(struct DbrSub));

    fprintf(stderr, "%zu large nodes per block:\n", pool->large.nodes_per_block);
    fprintf(stderr, "sizeof DbrRec=%zu\n", sizeof(struct DbrRec));
    fprintf(stderr, "sizeof DbrOrder=%zu\n", sizeof(struct DbrOrder));
    fprintf(stderr, "sizeof DbrTrade=%zu\n", sizeof(struct DbrTrade));
#endif // DBR_DEBUG_LEVEL >= 1
#endif // DBR_DEBUG_ALLOC
    if (!alloc_small_nodes(pool))
        goto fail1;

    if (!alloc_large_nodes(pool))
        goto fail2;

    return true;
 fail2:
    // Defensively does not assume single block.
    while (pool->small.first_block) {
        struct FigSmallBlock* block = pool->small.first_block;
        pool->small.first_block = block->next;
        free(block);
    }
 fail1:
    return false;
}

DBR_EXTERN void
fig_pool_term(struct FigPool* pool)
{
    assert(pool);

    // Large.
    while (pool->large.first_block) {
        struct FigLargeBlock* block = pool->large.first_block;
#if defined(DBR_DEBUG_ALLOC)
        for (int i = 0; i < pool->large.nodes_per_block; ++i) {
            if (block->nodes[i].file) {
                fprintf(stderr, "allocation in %s at %d\n",
                        block->nodes[i].file, block->nodes[i].line);
            }
        }
#endif // DBR_DEBUG_ALLOC
        pool->large.first_block = block->next;
        free(block);
    }
    pool->large.first_node = NULL;

    // Small.
    while (pool->small.first_block) {
        struct FigSmallBlock* block = pool->small.first_block;
#if defined(DBR_DEBUG_ALLOC)
        for (int i = 0; i < pool->small.nodes_per_block; ++i) {
            if (block->nodes[i].file) {
                fprintf(stderr, "allocation in %s at %d\n",
                        block->nodes[i].file, block->nodes[i].line);
            }
        }
#endif // DBR_DEBUG_ALLOC
        pool->small.first_block = block->next;
        free(block);
    }
    pool->small.first_node = NULL;

#if defined(DBR_DEBUG_ALLOC)
    fprintf(stderr, "%ld leaks detected\n", pool->allocs);
    fflush(stderr);
    assert(pool->allocs == 0 && pool->checksum == 0);
#endif // DBR_DEBUG_ALLOC
}

DBR_EXTERN struct FigSmallNode*
#if !defined(DBR_DEBUG_ALLOC)
fig_pool_alloc_small(struct FigPool* pool)
#else  // DBR_DEBUG_ALLOC
fig_pool_alloc_small(struct FigPool* pool, const char* file, int line)
#endif // DBR_DEBUG_ALLOC
{
    if (dbr_unlikely(!pool->small.first_node && !alloc_small_nodes(pool))) {
        dbr_err_set(DBR_ENOMEM, "out of memory");
        return false;
    }
    struct FigSmallNode* node = pool->small.first_node;
    pool->small.first_node = node->next;

#if defined(DBR_DEBUG_ALLOC)
    node->file = file;
    node->line = line;

    ++pool->allocs;
    pool->checksum ^= (unsigned long)node;
#endif // DBR_DEBUG_ALLOC
    return node;
}

DBR_EXTERN struct FigLargeNode*
#if !defined(DBR_DEBUG_ALLOC)
fig_pool_alloc_large(struct FigPool* pool)
#else  // DBR_DEBUG_ALLOC
fig_pool_alloc_large(struct FigPool* pool, const char* file, int line)
#endif // DBR_DEBUG_ALLOC
{
    if (dbr_unlikely(!pool->large.first_node && !alloc_large_nodes(pool))) {
        dbr_err_set(DBR_ENOMEM, "out of memory");
        return false;
    }
    struct FigLargeNode* node = pool->large.first_node;
    pool->large.first_node = node->next;

#if defined(DBR_DEBUG_ALLOC)
    node->file = file;
    node->line = line;

    ++pool->allocs;
    pool->checksum ^= (unsigned long)node;
#endif // DBR_DEBUG_ALLOC
    return node;
}

DBR_EXTERN void
fig_pool_free_small(struct FigPool* pool, struct FigSmallNode* node)
{
    if (node) {

        node->next = pool->small.first_node;
        pool->small.first_node = node;

#if defined(DBR_DEBUG_ALLOC)
        node->file = NULL;
        node->line = 0;

        --pool->allocs;
        pool->checksum ^= (unsigned long)node;

        assert(pool->allocs > 0 || (pool->allocs == 0 && pool->checksum == 0));
#endif // DBR_DEBUG_ALLOC
    }
}

DBR_EXTERN void
fig_pool_free_large(struct FigPool* pool, struct FigLargeNode* node)
{
    if (node) {

        node->next = pool->large.first_node;
        pool->large.first_node = node;

#if defined(DBR_DEBUG_ALLOC)
        node->file = NULL;
        node->line = 0;

        --pool->allocs;
        pool->checksum ^= (unsigned long)node;

        assert(pool->allocs > 0 || (pool->allocs == 0 && pool->checksum == 0));
#endif // DBR_DEBUG_ALLOC
    }
}

DBR_EXTERN void
fig_pool_free_matches(struct FigPool* pool, struct DbrSlNode* first)
{
    struct DbrSlNode* node = first;
    while (node) {
        struct DbrMatch* match = dbr_trans_match_entry(node);
        node = node->next;
        // Not committed so match object still owns the trades.
        fig_pool_free_trade(pool, match->taker_trade);
        fig_pool_free_trade(pool, match->maker_trade);
        fig_pool_free_match(pool, match);
    }
}

DBR_API DbrPool
dbr_pool_create(void)
{
    DbrPool pool = malloc(sizeof(struct FigPool));
    if (dbr_unlikely(!pool))
        goto fail1;

    if (dbr_unlikely(!fig_pool_init(pool)))
        goto fail2;

    return pool;
 fail2:
    free(pool);
 fail1:
    return NULL;
}

DBR_API void
dbr_pool_destroy(DbrPool pool)
{
    if (pool) {
        fig_pool_term(pool);
        free(pool);
    }
}

DBR_API struct DbrRec*
dbr_pool_alloc_rec(DbrPool pool)
{
    return fig_pool_alloc_rec(pool);
}

DBR_API void
dbr_pool_free_rec(DbrPool pool, struct DbrRec* rec)
{
    fig_pool_free_rec(pool, rec);
}

DBR_API struct DbrLevel*
dbr_pool_alloc_level(DbrPool pool, DbrKey key)
{
    return fig_pool_alloc_level(pool, key);
}

DBR_API void
dbr_pool_free_level(DbrPool pool, struct DbrLevel* level)
{
    fig_pool_free_level(pool, level);
}

DBR_API struct DbrMatch*
dbr_pool_alloc_match(DbrPool pool)
{
    return fig_pool_alloc_match(pool);
}

DBR_API void
dbr_pool_free_match(DbrPool pool, struct DbrMatch* match)
{
    fig_pool_free_match(pool, match);
}

DBR_API struct DbrOrder*
dbr_pool_alloc_order(DbrPool pool, DbrKey key)
{
    return fig_pool_alloc_order(pool, key);
}

DBR_API void
dbr_pool_free_order(DbrPool pool, struct DbrOrder* order)
{
    fig_pool_free_order(pool, order);
}

DBR_API struct DbrMemb*
dbr_pool_alloc_memb(DbrPool pool, DbrKey key)
{
    return fig_pool_alloc_memb(pool, key);
}

DBR_API void
dbr_pool_free_memb(DbrPool pool, struct DbrMemb* memb)
{
    fig_pool_free_memb(pool, memb);
}

DBR_API struct DbrTrade*
dbr_pool_alloc_trade(DbrPool pool, DbrKey key)
{
    return fig_pool_alloc_trade(pool, key);
}

DBR_API void
dbr_pool_free_trade(DbrPool pool, struct DbrTrade* trade)
{
    fig_pool_free_trade(pool, trade);
}

DBR_API struct DbrPosn*
dbr_pool_alloc_posn(DbrPool pool, DbrKey key)
{
    return fig_pool_alloc_posn(pool, key);
}

DBR_API void
dbr_pool_free_posn(DbrPool pool, struct DbrPosn* posn)
{
    fig_pool_free_posn(pool, posn);
}

DBR_API struct DbrSub*
dbr_pool_alloc_sub(DbrPool pool, DbrKey key)
{
    return fig_pool_alloc_sub(pool, key);
}

DBR_API void
dbr_pool_free_sub(DbrPool pool, struct DbrSub* sub)
{
    fig_pool_free_sub(pool, sub);
}