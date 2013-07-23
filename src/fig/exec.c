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
#include "exec.h"

#include "accnt.h"
#include "index.h"
#include "match.h"
#include "market.h"
#include "pool.h"
#include "trader.h"

#include <dbr/err.h>
#include <dbr/sess.h>

#include <stdbool.h>
#include <string.h>

static inline void
apply_posn(struct DbrPosn* posn, const struct DbrTrade* trade)
{
    const double licks = trade->lots * trade->ticks;
    if (trade->action == DBR_BUY) {
        posn->buy_licks += licks;
        posn->buy_lots += trade->lots;
    } else {
        assert(trade->action == DBR_SELL);
        posn->sell_licks += licks;
        posn->sell_lots += trade->lots;
    }
}

static inline DbrBool
update_order(struct FigExec* exec, struct DbrOrder* order, DbrMillis now)
{
    return dbr_journ_update_order(exec->journ, order->id, order->rev, order->status,
                                  order->resd, order->exec, order->lots, now);
}

static DbrBool
insert_trans(struct FigExec* exec, const struct DbrTrans* trans, DbrMillis now)
{
    struct DbrSlNode* node = trans->first_match;
    assert(node);

    const struct DbrOrder* taker_order = trans->new_order;
    int taker_rev = taker_order->rev;
    DbrLots taker_resd = taker_order->resd;
    DbrLots taker_exec = taker_order->exec;

    do {
        const struct DbrMatch* match = dbr_trans_match_entry(node);

        // Taker revision.
        ++taker_rev;
        taker_resd -= match->lots;
        taker_exec += match->lots;

        const int taker_status = taker_resd == 0 ? DBR_FILLED : DBR_PARTIAL;
        if (!dbr_journ_update_order(exec->journ, taker_order->id, taker_rev, taker_status,
                                    taker_resd, taker_exec, taker_order->lots, now)
            || !dbr_journ_insert_trade(exec->journ, match->taker_trade))
            goto fail1;

        // Maker revision.
        const struct DbrOrder* maker = match->maker_order;
        const int maker_rev = maker->rev + 1;
        const DbrLots maker_resd = maker->resd - match->lots;
        const DbrLots maker_exec = maker->exec + match->lots;
        const int maker_status = maker_resd == 0 ? DBR_FILLED : DBR_PARTIAL;
        if (!dbr_journ_update_order(exec->journ, maker->id, maker_rev, maker_status,
                                    maker_resd, maker_exec, maker->lots, now)
            || !dbr_journ_insert_trade(exec->journ, match->maker_trade))
            goto fail1;

    } while ((node = node->next));
    return true;
 fail1:
    return false;
}

// Assumes that maker lots have not been reduced since matching took place.

static void
apply_trades(struct FigExec* exec, struct FigMarket* market, const struct DbrTrans* trans,
             DbrMillis now)
{
    const struct DbrOrder* taker_order = trans->new_order;
    // Must succeed because new_posn exists.
    struct FigAccnt* taker_accnt = fig_accnt_lazy(taker_order->accnt.rec, exec->pool);
    assert(taker_accnt);

    for (struct DbrSlNode* node = trans->first_match; node; node = node->next) {

        struct DbrMatch* match = dbr_trans_match_entry(node);
        struct DbrOrder* maker_order = match->maker_order;

        // Reduce maker. Maker's revision will be incremented by this call.
        fig_market_take(market, maker_order, match->lots, now);

        // Must succeed because maker_posn exists.
        struct FigAccnt* maker_accnt = fig_accnt_lazy(maker_order->accnt.rec, exec->pool);
        assert(maker_accnt);

        // Update taker.
        fig_accnt_emplace_trade(taker_accnt, match->taker_trade);
        apply_posn(trans->new_posn, match->taker_trade);

        // Update maker.
        fig_accnt_emplace_trade(maker_accnt, match->maker_trade);
        apply_posn(match->maker_posn, match->maker_trade);
        // Async trader callback.
        dbr_accnt_sess_trade(fig_accnt_sess(maker_accnt), maker_order, match->maker_trade,
                             match->maker_posn);
    }
}

DBR_EXTERN void
fig_exec_init(struct FigExec* exec, struct FigPool* pool, DbrJourn journ, struct FigIndex* index)
{
    exec->pool = pool;
    exec->journ = journ;
    exec->index = index;
}

DBR_EXTERN struct DbrOrder*
fig_exec_submit(struct FigExec* exec, struct DbrRec* trec, struct DbrRec* arec, const char* ref,
                struct DbrRec* mrec, int action, DbrTicks ticks, DbrLots lots, DbrLots min,
                DbrFlags flags, DbrMillis now, struct DbrTrans* trans)
{
    struct FigTrader* trader = fig_trader_lazy(trec, exec->pool, exec->index);
    if (!trader)
        goto fail1;

    struct FigMarket* market = fig_market_lazy(mrec, exec->pool);
    if (!market)
        goto fail1;

    const DbrIden id = dbr_journ_alloc_id(exec->journ);
    struct DbrOrder* new_order = fig_pool_alloc_order(exec->pool, id);
    if (!new_order)
        goto fail1;

    new_order->id = id;
    new_order->level = NULL;
    new_order->rev = 1;
    new_order->status = DBR_NEW;
    new_order->trader.rec = trec;
    new_order->accnt.rec = arec;
    if (ref)
        strncpy(new_order->ref, ref, DBR_REF_MAX);
    else
        new_order->ref[0] = '\0';

    new_order->market.rec = mrec;
    new_order->action = action;
    new_order->ticks = ticks;
    new_order->resd = lots;
    new_order->exec = 0;
    new_order->lots = lots;
    new_order->min = min;
    new_order->flags = flags;
    new_order->created = now;
    new_order->modified = now;

    trans->new_order = new_order;
    trans->new_posn = NULL;

    if (!dbr_journ_begin_trans(exec->journ))
        goto fail2;

    if (!dbr_journ_insert_order(exec->journ, new_order)
        || !fig_match_orders(exec->pool, exec->journ, market, new_order, trans))
        goto fail3;

    if (trans->count > 0) {

        // Orders were matched.
        if (!insert_trans(exec, trans, now))
            goto fail4;

        // Commit taker order.
        new_order->rev += trans->count;
        new_order->resd -= trans->taken;
        new_order->exec += trans->taken;

        new_order->status = dbr_order_done(new_order) ? DBR_FILLED : DBR_PARTIAL;
    }

    // Place incomplete order in book.
    if (!dbr_order_done(new_order) && !fig_market_insert(market, new_order))
        goto fail4;

    // Commit phase cannot fail.
    fig_trader_emplace_order(trader, new_order);
    apply_trades(exec, market, trans, now);
    dbr_journ_commit_trans(exec->journ);
    return new_order;
 fail4:
    fig_pool_free_matches(exec->pool, trans->first_match);
    memset(trans, 0, sizeof(*trans));
 fail3:
    dbr_journ_rollback_trans(exec->journ);
 fail2:
    fig_pool_free_order(exec->pool, new_order);
 fail1:
    memset(trans, 0, sizeof(*trans));
    return NULL;
}

DBR_EXTERN struct DbrOrder*
fig_exec_revise_id(struct FigExec* exec, struct FigTrader* trader, DbrIden id, DbrLots lots,
                   DbrMillis now)
{
    struct DbrRbNode* node = fig_trader_find_order_id(trader, id);
    if (!node) {
        dbr_err_set(DBR_EINVAL, "no such order '%ld'", id);
        goto fail1;
    }

    struct DbrOrder* order = dbr_trader_order_entry(node);
    if (dbr_order_done(order)) {
        dbr_err_set(DBR_EINVAL, "order complete '%ld'", id);
        goto fail1;
    }

    if (!dbr_journ_begin_trans(exec->journ))
        goto fail1;

    if (!dbr_journ_update_order(exec->journ, id, order->rev + 1, DBR_REVISED, order->resd,
                                order->exec, lots, now))
        goto fail2;

    // Must succeed because order exists.
    struct FigMarket* market = fig_market_lazy(order->market.rec, exec->pool);
    assert(market);

    if (!fig_market_revise(market, order, lots, now))
        goto fail2;

    dbr_journ_commit_trans(exec->journ);
    return order;
 fail2:
    dbr_journ_rollback_trans(exec->journ);
 fail1:
    return NULL;
}

DBR_EXTERN struct DbrOrder*
fig_exec_revise_ref(struct FigExec* exec, struct FigTrader* trader, const char* ref, DbrLots lots,
                    DbrMillis now)
{
    struct DbrOrder* order = fig_trader_find_order_ref(trader, ref);
    if (!order) {
        dbr_err_set(DBR_EINVAL, "no such order '%.64s'", ref);
        goto fail1;
    }

    if (dbr_order_done(order)) {
        dbr_err_set(DBR_EINVAL, "order complete '%.64s'", ref);
        goto fail1;
    }

    if (!dbr_journ_begin_trans(exec->journ))
        goto fail1;

    if (!dbr_journ_update_order(exec->journ, order->id, order->rev + 1, DBR_REVISED, order->resd,
                                order->exec, lots, now))
        goto fail2;

    struct FigMarket* market = fig_market_lazy(order->market.rec, exec->pool);
    assert(market);
    if (!fig_market_revise(market, order, lots, now))
        goto fail2;

    dbr_journ_commit_trans(exec->journ);
    return order;
 fail2:
    dbr_journ_rollback_trans(exec->journ);
 fail1:
    return NULL;
}

DBR_EXTERN struct DbrOrder*
fig_exec_cancel_id(struct FigExec* exec, struct FigTrader* trader, DbrIden id, DbrMillis now)
{
    struct DbrRbNode* node = fig_trader_find_order_id(trader, id);
    if (!node) {
        dbr_err_set(DBR_EINVAL, "no such order '%ld'", id);
        goto fail1;
    }

    struct DbrOrder* order = dbr_trader_order_entry(node);
    if (dbr_order_done(order)) {
        dbr_err_set(DBR_EINVAL, "order complete '%ld'", id);
        goto fail1;
    }

    if (!dbr_journ_update_order(exec->journ, id, order->rev + 1, DBR_CANCELLED, 0,
                                order->exec, order->lots, now))
        goto fail1;

    struct FigMarket* market = fig_market_lazy(order->market.rec, exec->pool);
    assert(market);
    fig_market_cancel(market, order, now);
    return order;
 fail1:
    return NULL;
}

DBR_EXTERN struct DbrOrder*
fig_exec_cancel_ref(struct FigExec* exec, struct FigTrader* trader, const char* ref, DbrMillis now)
{
    struct DbrOrder* order = fig_trader_find_order_ref(trader, ref);
    if (!order) {
        dbr_err_set(DBR_EINVAL, "no such order '%.64s'", ref);
        goto fail1;
    }

    if (dbr_order_done(order)) {
        dbr_err_set(DBR_EINVAL, "order complete '%.64s'", ref);
        goto fail1;
    }

    if (!dbr_journ_update_order(exec->journ, order->id, order->rev + 1, DBR_CANCELLED, 0,
                                order->exec, order->lots, now))
        goto fail1;

    struct FigMarket* market = fig_market_lazy(order->market.rec, exec->pool);
    assert(market);
    fig_market_cancel(market, order, now);
    return order;
 fail1:
    return NULL;
}

DBR_EXTERN DbrBool
fig_exec_archive_order(struct FigExec* exec, struct FigTrader* trader, DbrIden id, DbrMillis now)
{
    struct DbrRbNode* node = fig_trader_find_order_id(trader, id);
    if (!node)
        goto fail1;

    if (!dbr_journ_archive_order(exec->journ, node->key, now))
        goto fail1;

    // No need to update timestamps on trade because it is immediately freed.

    struct DbrOrder* order = dbr_trader_order_entry(node);
    fig_trader_release_order(trader, order);
    fig_pool_free_order(exec->pool, order);
    return true;
 fail1:
    return false;
}

DBR_EXTERN DbrBool
fig_exec_archive_trade(struct FigExec* exec, struct FigAccnt* accnt, DbrIden id, DbrMillis now)
{
    struct DbrRbNode* node = fig_accnt_find_trade_id(accnt, id);
    if (!node)
        goto fail1;

    if (!dbr_journ_archive_trade(exec->journ, node->key, now))
        goto fail1;

    // No need to update timestamps on trade because it is immediately freed.

    struct DbrTrade* trade = dbr_accnt_trade_entry(node);
    fig_accnt_release_trade(accnt, trade);
    fig_pool_free_trade(exec->pool, trade);
    return true;
 fail1:
    return false;
}

DBR_EXTERN void
fig_exec_free_matches(struct FigExec* exec, struct DbrSlNode* first)
{
    struct DbrSlNode* node = first;
    while (node) {
        struct DbrMatch* match = dbr_trans_match_entry(node);
        node = node->next;
        fig_pool_free_match(exec->pool, match);
    }
}