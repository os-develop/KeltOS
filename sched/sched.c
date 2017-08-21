#include "kernel/defs.h"
#include "sched/sched.h"
#include "kernel/alloc.h"
#include "kernel/memory.h"
#include "kernel/printk.h"
#include "kernel/bug.h"
#include "drivers/nvic.h"
#include "kernel/timer.h"
#include "kernel/syscall.h"
#include "kernel/irq.h"

int sched_enabled = 0;
struct task tasks[MAX_TASKS];
struct task* c_task = NULL;

LIST_HEAD_DEFINE(high_priority_list);
LIST_HEAD_DEFINE(normal_priority_list);
LIST_HEAD_DEFINE(low_priority_list);

/* TODO: current realization lacks synchoronization */

static void sched_insert_task(struct task* task)
{
    switch (task_priority(task)) {
    case PRIORITY_HIGH:
        list_insert_last(&high_priority_list, &task->lnode);
        break;
    case PRIORITY_NORMAL:
        list_insert_last(&normal_priority_list, &task->lnode);
        break;
    case PRIORITY_LOW:
        list_insert_last(&low_priority_list, &task->lnode);
        break;
    }
}

struct task* sched_start_task(void* start_address, int priority)
{
    struct task* task = NULL;
    for (u32 i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].pid == 0) {
            task = tasks + i;
            task->pid = i + 1;
            break;
        }
    }
    if (task != NULL) {
        /* allocate only one region for all stacks? */
        void* sp_top = kalloc(TASK_STACK_SIZE);

        if (sp_top == NULL) {
            /* no space for task */
            return NULL;
        }

        void* sp = sp_top + TASK_STACK_SIZE;
        struct task_context_exc* exc_ctx = sp - sizeof(struct task_context_exc);
        struct task_context* ctx = sp - sizeof(struct task_context_exc) - sizeof(struct task_context);
        /* prepare stack pointer for first context restore */
        sp = ctx;

        /* gcc produces addresses with non-zero last bit to set EPSR.T bit */
        exc_ctx->pc = ((u32)start_address) & ~(u32)0x1;
        exc_ctx->psr = DEFAULT_PSR;

        task->sp = sp;
        task_set_priority(task, priority);
        task_set_state(task, TASK_RUNNING);

        sched_insert_task(task);
    }

    return task;
}

struct task* sched_switch_task()
{
    c_task = NULL;
    if (!list_empty(&high_priority_list)) {
        c_task = list_first_entry(high_priority_list, struct task, lnode);
        list_rotate_left(&high_priority_list);
    } else if (!list_empty(&normal_priority_list)) {
        c_task = list_first_entry(normal_priority_list, struct task, lnode);
        list_rotate_left(&normal_priority_list);
    } else if (!list_empty(&low_priority_list)) {
        c_task = list_first_entry(low_priority_list, struct task, lnode);
        list_rotate_left(&low_priority_list);
    }

    return c_task;
}

void sched_task_set_sleeping(struct task* task)
{
    list_delete(&task->lnode);
    task_set_state(task, TASK_SLEEPING);
}

void sched_task_wake_up(struct task* task)
{
    if (task_state(task) == TASK_SLEEPING) {
        task_set_state(task, TASK_RUNNING);
        sched_insert_task(task);
    }
}

s32 sys_exit(struct sys_params* params)
{
    BUG_ON_NULL(c_task);
    list_delete(&c_task->lnode);
    c_task->pid = 0;
    sched_context_switch();

    return KELT_OK;
}

s32 sys_yield(struct sys_params* params)
{
    BUG_ON_NULL(c_task);
    sched_context_switch();

    return KELT_OK;
}

void sched_start()
{
    sched_enabled = 1;
    c_task = sched_switch_task();
    /* emulate enter from process stack */
    c_task->sp += sizeof(struct task_context);
    set_psp((u32)c_task->sp);
    sched_context_switch();
}