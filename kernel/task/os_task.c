#include "os_task_external.h"
#include "os_list_external.h"
#include "os_def.h"

OS_SEC_L1_BSS struct os_task_cb g_task_cb_array[OS_TASK_MAX_NUM];
OS_SEC_L2_DATA struct os_list g_task_cb_free_list = OS_LIST_INIT(&g_task_cb_free_list);

OS_SEC_L2_TEXT void os_task_config(void)
{
    uint32_t i;
    struct os_task_cb *task_cb;

    for (i = 0; i < OS_TASK_MAX_NUM; i++) {
        task_cb = &g_task_cb_array[i];

        task_cb->status = OS_TASK_NOT_CREATE;
        os_list_add_tail(&g_task_cb_free_list, &task_cb->free_list_node);
        os_list_init(&task_cb->delay_list_node);
        os_list_init(&task_cb->pend_list_node);
        os_list_init(&task_cb->rdy_list_node);
        os_list_init(&task_cb->sem_list);
    }  
}