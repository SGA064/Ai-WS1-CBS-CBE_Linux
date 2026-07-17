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
OAL_STATIC osal_void hmac_scan_info_report(struct seq_file *f)
{
    hmac_device_stru        *hmac_device;
    hmac_bss_mgmt_stru      *bss_mgmt;
    hmac_scanned_bss_info   *scanned_bss;
    mac_bss_dscr_stru       *bss_dscr;
    struct osal_list_head   *entry;

    hmac_device = hmac_res_get_mac_dev_etc(0);
    if (hmac_device == OSAL_NULL) {
        return;
    }

    bss_mgmt = &(hmac_device->scan_mgmt.scan_record_mgmt.bss_mgmt);

    osal_spin_lock(&(bss_mgmt->lock));

    seq_printf(f, "==================SCAN INFO==================\n");
    seq_printf(f, "MAC ADDR / rssi / channel / SSID\n");
    osal_list_for_each(entry, &(bss_mgmt->bss_list_head)) {
        scanned_bss = osal_list_entry(entry, hmac_scanned_bss_info, dlist_head);
        bss_dscr = &(scanned_bss->bss_dscr_info);

        if (hmac_device->band_cap == WLAN_BAND_CAP_2G &&
            bss_dscr->st_channel.chan_number > 14) { /* 2g频段 超出14信道不打印 */
            continue;
        } else if (hmac_device->band_cap == WLAN_BAND_CAP_5G &&
            bss_dscr->st_channel.chan_number < 36) { /* 5g频段 低于36信道不打印 */
            continue;
        }

        seq_printf(f, "%02X:%02X:%02X:%02X:XX:XX\t",
            bss_dscr->auc_mac_addr[0], bss_dscr->auc_mac_addr[1], /* MAC地址的第0字节.第1字节 */
            bss_dscr->auc_mac_addr[2], bss_dscr->auc_mac_addr[3]); /* MAC地址的第2字节.第3字节 */
        seq_printf(f, "%d\t", bss_dscr->c_rssi);
        seq_printf(f, "%d\t", bss_dscr->st_channel.chan_number);
        seq_printf(f, "%s\n", bss_dscr->ac_ssid);
    }

    osal_spin_unlock(&(bss_mgmt->lock));
    return;
}

OAL_STATIC osal_void hmac_conn_info_report(struct seq_file *f)
{
    char *bandwidth_str[] = {"20M", "40M", "40M", "80M", "80M", "80M", "80M", "5M", "10M", "40M", "80M", "error"};
    char *negotiate_str[] = {"11a", "11b", "11g", "11bg", "11g", "11n", "11ac", "11n", "11ac", "11ag", "11ax", "error"};
    hmac_device_stru *hmac_device = hmac_res_get_mac_dev_etc(0);
    hmac_vap_stru *hmac_vap = OSAL_NULL;
    hmac_user_stru *hmac_user = OSAL_NULL;
    struct osal_list_head *entry = OSAL_NULL;
    struct osal_list_head *dlist_tmp = OSAL_NULL;
    frw_msg cfg_info;
    mac_cfg_ar_tx_params_stru ar_tx_params;
    mac_cfg_ar_tx_params_stru rsp_info;
    osal_u8 vap_idx;
    osal_s32 ret;

    seq_printf(f, "==================CONN INFO==================\n");
    /* 遍历device下所有vap */
    for (vap_idx = 0; vap_idx < hmac_device->vap_num; vap_idx++) {
        hmac_vap = (hmac_vap_stru *)mac_res_get_hmac_vap(hmac_device->vap_id[vap_idx]);
        if (hmac_vap == OSAL_NULL) {
            continue;
        }
        osal_list_for_each_safe(entry, dlist_tmp, &(hmac_vap->mac_user_list_head)) {
            hmac_user = osal_list_entry(entry, hmac_user_stru, user_dlist);
            if (hmac_user == OSAL_NULL) {
                continue;
            }

            (osal_void)memset_s(&cfg_info, sizeof(frw_msg), 0, sizeof(frw_msg));
            (osal_void)memset_s(&ar_tx_params, sizeof(mac_cfg_ar_tx_params_stru), 0, sizeof(mac_cfg_ar_tx_params_stru));
            (osal_void)memset_s(&rsp_info, sizeof(mac_cfg_ar_tx_params_stru), 0, sizeof(mac_cfg_ar_tx_params_stru));
            ar_tx_params.user_id = hmac_user->assoc_id;
            cfg_msg_init((osal_u8 *)&ar_tx_params, OAL_SIZEOF(mac_cfg_ar_tx_params_stru),
                (osal_u8 *)&rsp_info, OAL_SIZEOF(mac_cfg_ar_tx_params_stru), &cfg_info);
            ret = send_cfg_to_device(hmac_vap->vap_id, WLAN_MSG_H2D_C_CFG_GET_TX_PARAMS, &cfg_info, OSAL_TRUE);
            if ((ret != OAL_SUCC) || (cfg_info.rsp == OSAL_NULL)) {
                continue;
            }

            if (hmac_vap->channel.en_bandwidth >= oal_array_size(bandwidth_str) ||
                hmac_user->avail_protocol_mode >= oal_array_size(negotiate_str)) {
                continue;
            }

            seq_printf(f, "[mac addr]:%02X:%02X:%02X:%02X:XX:XX, assoc_id:%d\n",
                hmac_user->user_mac_addr[0], hmac_user->user_mac_addr[1], // 0, 1 is mac_addr
                hmac_user->user_mac_addr[2], hmac_user->user_mac_addr[3], // 2, 3 is mac_addr
                hmac_user->assoc_id);
            seq_printf(f, "[connection info]: channel:%d, bandwidth:%s, protocol:%s, rate:%dkbps, rssi:%d\n",
                hmac_vap->channel.chan_number, bandwidth_str[hmac_vap->channel.en_bandwidth],
                negotiate_str[hmac_user->avail_protocol_mode], rsp_info.tx_best_rate,
                oal_get_real_rssi(hmac_user->rx_rssi));
        }
    }
}

OAL_STATIC osal_void hmac_conn_intf_info_report(struct seq_file *f)
{
    hmac_device_stru *hmac_device = OSAL_NULL;
    hmac_scan_stru *scan_mgmt = OSAL_NULL;
    hmac_scan_record_stru *scan_record = OSAL_NULL;
    wlan_scan_chan_stats_stru *chan_stats = OSAL_NULL;
    hal_alg_intf_det_mode_enum_uint8 adjch_intf_type;
    oal_bool_enum_uint8 coch_intf_type;
    osal_s8 intf_det_avg_rssi_20_scan;
    osal_s8 intf_det_avg_rssi_20_conn;
    osal_u8 idx;
    osal_u32 ret;

    seq_printf(f, "==================INTF INFO==================\n");
    ret = hmac_alg_get_intf_info(&adjch_intf_type, &coch_intf_type, &intf_det_avg_rssi_20_conn);
    if (ret == OAL_SUCC) {
        seq_printf(f, "[connected intf info]: coch_intf_type = %d, adjch_intf_type = %d, noise_floor = %d\n",
            coch_intf_type, adjch_intf_type, intf_det_avg_rssi_20_conn);
    }

    hmac_device = hmac_res_get_mac_dev_etc(0);
    if (osal_unlikely(hmac_device == OSAL_NULL)) {
        return;
    }

    scan_mgmt = &(hmac_device->scan_mgmt);
    scan_record = &(scan_mgmt->scan_record_mgmt);
    chan_stats = scan_record->chan_results;

    /* 检测本次扫描是否开启了信道测量，如果没有直接返回 */
    if (chan_stats[0].stats_valid == 0) {
        return;
    }

    /* 打印信道测量结果 */
    for (idx = 0; idx < scan_record->chan_numbers; idx++) {
        /* 获取统计的信道结果:底噪平均值,如果统计计数为0则不做除法 */
        if (chan_stats[idx].free_power_cnt == 0) {
            intf_det_avg_rssi_20_scan = 0;
        } else {
            intf_det_avg_rssi_20_scan =
                (osal_s8)(chan_stats[idx].free_power_stats_20m / (osal_s16)chan_stats[idx].free_power_cnt);
        }
        seq_printf(f, "[scan intf info]: chan_num = %d, noise_floor = %d\n", chan_stats[idx].channel_number,
            intf_det_avg_rssi_20_scan);
    }
}

osal_s32 hmac_wlan_info_report_proc(hmac_vap_stru *hmac_vap, frw_msg *msg)
{
    struct seq_file *f = (struct seq_file *)(uintptr_t)(*(uintptr_t *)msg->data);
    unref_param(hmac_vap);

    /* 扫描信息上报 */
    hmac_scan_info_report(f);
    /* 关联信息上报 */
    hmac_conn_info_report(f);
    /* 干扰信息上报 */
    hmac_conn_intf_info_report(f);

    return OAL_SUCC;
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* _PRE_WLAN_INFO_REPORT */