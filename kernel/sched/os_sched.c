#include "os_sched_external.h"
#include "os_list_external.h"

OS_SEC_L1_BSS struct os_run_que g_run_que;

/* 外部关中断 */
OS_SEC_L1_TEXT void os_sched_enqueue_rdy_list_tail(struct os_task_cb *tsk)
{
    struct os_run_que *rq = OS_RUN_QUE();
    uint32_t tsk_prio = tsk->prio;
    struct os_list *rdy_list = &rq->rdy_list[tsk_prio];

    os_list_add_tail(rdy_list, &tsk->rdy_list_node);

    if (tsk_prio < rq->cur_prio) {
        rq->cur_prio = tsk_prio;
        rq->need_sched = TRUE;
    }
}

OS_SEC_L1_TEXT void os_sched_dequeue_rdy_list(struct os_task_cb* tsk)
{
    struct os_run_que *rq = OS_RUN_QUE();
    struct os_list *rdy_list = &rq->rdy_list[tsk->prio];

    os_list_remove_node(&tsk->rdy_list_node);

    if (tsk == rq->running_task) {
        rq->need_sched = TRUE;
    }
}