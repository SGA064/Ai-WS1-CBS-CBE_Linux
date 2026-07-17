/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: Header file for hmac_net_diagnosis.c
 * Create: 2024-11-7
 */

#ifndef __HMAC_NET_DIAGNOSIS_H__
#define __HMAC_NET_DIAGNOSIS_H__

#include "hmac_vap.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef struct {
    osal_u64 start_timestamp;
    osal_u64 bcn_cnt;
    osal_u8 record_flag;
} hmac_beacon_count_stru;

typedef struct {
    osal_u8 nss_rate : 6;
    osal_u8 nss_mode : 2;
    osal_u8 protocol;
    osal_s8 snr_ant0;
    osal_s8 snr_ant1;
    osal_u8 freq_bw;
    osal_u8 freq_bw_mode;
} hmac_usr_rx_info_stru;

static osal_u32 hmac_net_diagnosis_init_weakref(osal_void) __attribute__ ((weakref("hmac_net_diagnosis_init"), used));
static osal_void hmac_net_diagnosis_deinit_weakref(osal_void) __attribute__ ((weakref("hmac_net_diagnosis_deinit"),
    used));

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif