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
#ifndef DBR_PRIOQ_H
#define DBR_PRIOQ_H

#include <dbr/defs.h>

#include <stddef.h> // size_t

/**
 * @addtogroup Prioq
 * @{
 */

struct DbrPair {
    long key;
    long val;
};

struct DbrPrioq {
    size_t size;
    size_t capacity;
    struct DbrPair* elems;
};

DBR_API void
dbr_prioq_term(struct DbrPrioq* pq);

DBR_API DbrBool
dbr_prioq_init(struct DbrPrioq* pq);

DBR_API DbrBool
dbr_prioq_push(struct DbrPrioq* pq, long key, long val);

DBR_API DbrBool
dbr_prioq_pop(struct DbrPrioq* pq, struct DbrPair* elem);

/** @} */

#endif // DBR_PRIOQ_H