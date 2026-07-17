/*
 * Copyright (c) @CompanyNameMagicTag 2024-2024. All rights reserved.
 * Description: This module defines the security random num get for gle.
 * Create: 2024-05-31
 */
#include <linux/uaccess.h>
#include "securec.h"
#include "soc_osal.h"
#include "oal_types.h"
#include "trng.h"
#include "sle_dev_random.h"

long sle_send_random_msg(uint8_t *random)
{
    uint8_t         random_bytes[DEVICE_RANDOM_LENGTH] = { 0 };
    osal_s32        ret = EXT_ERR_SUCCESS;

    ret = uapi_drv_cipher_trng_get_random_bytes(random_bytes, DEVICE_RANDOM_LENGTH);
    if (ret != OAL_SUCC) {
        printk(KERN_ERR"uapi_drv_cipher_trng_get_random_bytes failed, 0x%x.", ret);
        return ret;
    }
    return copy_to_user((void*)random, random_bytes, DEVICE_RANDOM_LENGTH);
}