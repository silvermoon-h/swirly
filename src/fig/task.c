/*
 *  Copyright (C) 2013, 2014 Mark Aylett <mark.aylett@gmail.com>
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
#include <dbr/fig/task.h>

struct Task {
    int (*fn)(DbrHandler, DbrClnt, void*);
    void* arg;
};

DBR_API int
dbr_task_call(DbrAsync async, int (*fn)(DbrHandler, DbrClnt, void*), void* arg)
{
    struct Task t = { .fn = fn, .arg = arg };
    dbr_async_send(async, &t);
    void* val;
    dbr_async_recv(async, &val);
    return (int)(long)val;
}

DBR_API void*
dbr_task_on_async(DbrHandler handler, DbrClnt clnt, void* val)
{
    struct Task* t = val;
    return (void*)(long)t->fn(handler, clnt, t->arg);
}
