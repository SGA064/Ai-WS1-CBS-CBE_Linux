/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2020-2024. All rights reserved.
 * Description: wlan信息上报文件.
 */

#ifdef _PRE_WLAN_INFO_REPORT

/*****************************************************************************
  1 头文件包含
*****************************************************************************/
#include "hmac_wlan_info_report.h"
#include "hmac_device.h"
#include "hmac_alg_notify.h"
#include "mac_resource_ext.h"
#include "wlan_msg.h"
#include "wal_common.h"
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,59))
#include "../fs/proc/internal.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 全局变量定义
*****************************************************************************/

/*****************************************************************************
  3 函数实现
*****************************************************************************/
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,59))

OAL_STATIC osal_s32 wal_wlan_info_report_seq_show(struct seq_file *f, void *v)
{
    osal_s32 ret;

    ret = wal_sync_post2hmac_no_rsp(0, WLAN_MSG_W2H_C_CFG_WLAN_INFO_REPORT, (osal_u8 *)&f,
        OAL_SIZEOF(struct seq_file *));
    return ret;
}

OAL_STATIC osal_s32 wal_wlan_info_report_proc_open(struct inode *inode, struct file *filp)
{
    osal_s32               ret;
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
    struct proc_dir_entry  *pde = PDE(inode);

    ret = single_open(filp, wal_wlan_info_report_seq_show, pde->data);
#else
    ret = single_open(filp, wal_wlan_info_report_seq_show, PDE_DATA(inode));
#endif
    return ret;
}

osal_u8 g_wlan_info_report_flag = OSAL_FALSE;
OAL_STATIC oal_proc_dir_entry_stru *g_info_proc_dir = OSAL_NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
OAL_STATIC const struct file_operations g_wlan_info_report = {
    .owner      = THIS_MODULE,
    .open       = wal_wlan_info_report_proc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
#else
OAL_STATIC const struct proc_ops g_wlan_info_report = {
    .proc_open       = wal_wlan_info_report_proc_open,
    .proc_read       = seq_read,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
#endif
#endif

osal_s32 wal_config_wlan_info_report(osal_u8 flag)
{
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,59)) /* 仅支持高版本linux内核 */
    oal_proc_dir_entry_stru *proc_info = OSAL_NULL;
    char info_dir[] = "ws73";

    if (flag == g_wlan_info_report_flag) {
        return OAL_SUCC;
    }

    if (flag == OSAL_TRUE) { /* 创建节点记录环境信息 */
        if (g_info_proc_dir != OSAL_NULL) {
            return OAL_SUCC;
        }

        g_info_proc_dir = proc_mkdir(info_dir, init_net.proc_net);
        if (g_info_proc_dir == OSAL_NULL) {
            oam_warning_log0(0, OAM_SF_CFG, "hmac_config_wlan_info_report: proc_mkdir return null");
            return OAL_FAIL;
        }

        proc_info = proc_create("wlan0", 420, g_info_proc_dir, &g_wlan_info_report); /* 权限:420 */
        if (proc_info == OSAL_NULL) {
            oal_remove_proc_entry(info_dir, NULL);
            oam_warning_log0(0, OAM_SF_CFG, "hmac_config_wlan_info_report: proc_create return null");
            return OAL_FAIL;
        }
    } else {
        if (g_info_proc_dir == OSAL_NULL) {
            return OAL_SUCC;
        }
        oal_remove_proc_entry("wlan0", g_info_proc_dir);
        oal_remove_proc_entry(info_dir, init_net.proc_net);
        g_info_proc_dir = OSAL_NULL;
    }

    g_wlan_info_report_flag = flag;
#else
    unref_param(flag);
#endif
    return OAL_SUCC;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* _PRE_WLAN_INFO_REPORT */