#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include "argparsing.h"
#include "qthread_innards.h"
#include "qt_shepherd_innards.h"

static aligned_t t  = 1;
static aligned_t t2 = 1;
static aligned_t t3 = 1;

static aligned_t waiter_count = 0;

aligned_t alive[6];

static void alive_check(const size_t a, const size_t b, void *junk)
{
    return;
}

static aligned_t live_waiter(void *arg)
{
    const int assigned = (int)(intptr_t)arg;
    const int id = qthread_id();
    iprintf("live_waiter %i alive! id %i wkr %u\n", assigned, id, qthread_readstate(CURRENT_UNIQUE_WORKER));
    qthread_fill(&alive[assigned]);
    qthread_flushsc();
    while(t == 1) {
	COMPILER_FENCE;
    }
    qthread_incr(&waiter_count, 1);
    iprintf("live_waiter %i exiting! id %i wkr %u\n", assigned, id, qthread_readstate(CURRENT_UNIQUE_WORKER));

    return 0;
}

static aligned_t live_parent(void *arg)
{
    iprintf("live_parent alive!\n");
    qthread_empty(alive+0);
    qthread_empty(alive+1);
    qthread_fork(live_waiter, (void*)(intptr_t)0, &t3);
    qthread_fork(live_waiter, (void*)(intptr_t)1, &t3);
    iprintf("live_parent waiting on %p\n", &alive[1]);
    qthread_readFF(NULL, &alive[1]);
    iprintf("saw live_waiter 1 report in\n");
    iprintf("live_parent waiting on %p\n", &alive[0]);
    qthread_readFF(NULL, &alive[0]);
    iprintf("saw live_waiter 0 report in\n");
    iprintf("live_parent about to eureka...\n");
    qt_team_eureka();
    iprintf("live_parent still alive!\n");
    COMPILER_FENCE;
    t = 0;
    return 0;
}

static aligned_t live_waiter2(void *arg)
{
    const int assigned = (int)(intptr_t)arg;
    const int id = qthread_id();
    qt_team_critical_section(BEGIN);
    iprintf("live_waiter %i alive! id %i wkr %u\n", assigned, id, qthread_readstate(CURRENT_UNIQUE_WORKER));
    qt_team_critical_section(END);
    while(t == 1) {
	COMPILER_FENCE;
    }
    qthread_incr(&waiter_count, 1);
    qt_team_critical_section(BEGIN);
    iprintf("live_waiter %i exiting! id %i wkr %u\n", assigned, id, qthread_readstate(CURRENT_UNIQUE_WORKER));
    qt_team_critical_section(END);

    return 0;
}

static aligned_t live_parent2(void *arg)
{
    iprintf("live_parent alive!\n");
    qthread_fork(live_waiter2, (void*)(intptr_t)0, &t3);
    qthread_fork(live_waiter2, (void*)(intptr_t)1, &t3);
    qthread_fork(live_waiter2, (void*)(intptr_t)2, &t3);
    qthread_fork(live_waiter2, (void*)(intptr_t)3, &t3);
    qthread_fork(live_waiter2, (void*)(intptr_t)4, &t3);
    qthread_fork(live_waiter2, (void*)(intptr_t)5, &t3);
    iprintf("live_parent spawned all tasks\n");
    qthread_flushsc();
    iprintf("live_parent about to eureka...\n");
    qt_team_eureka();
    iprintf("live_parent still alive!\n");
    COMPILER_FENCE;
    t = 0;
    return 0;
}

int main(int   argc,
         char *argv[])
{
    int ret = 0;

    ret = qthread_init(3);
    if (ret != QTHREAD_SUCCESS) {
	fprintf(stderr, "initialization error\n");
	abort();
    }

    CHECK_VERBOSE();

    iprintf("%i shepherds...\n", qthread_num_shepherds());
    iprintf("  %i threads total\n", qthread_num_workers());

    qt_loop_balance(0, qthread_num_workers(), alive_check, NULL);

    iprintf("Testing a fully-live eureka (all member tasks running)...\n");
    qthread_fork_new_team(live_parent, NULL, &t2);
    qthread_readFF(NULL, &t2);
    assert(waiter_count == 0);
    t = 1;

    iprintf("\n\n***************************************************************\n");
    iprintf("Testing a partially-live eureka (some member tasks running)...\n");
    qthread_fork_new_team(live_parent2, NULL, &t2);
    qthread_readFF(NULL, &t2);
    assert(waiter_count == 0);
    t = 1;

    iprintf("Success!\n");

    return 0;
}

/* vim:set expandtab */