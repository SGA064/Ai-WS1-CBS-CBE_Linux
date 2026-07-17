/*
 * Copyright (c) @CompanyNameMagicTag 2024-2024. All rights reserved.
 * Description: This module defines the security random num get for gle.
 * Create: 2024-05-31
 */

#ifndef SLE_DEV_RANDOM_H_
#define SLE_DEV_RANDOM_H_
long sle_send_random_msg(uint8_t *random);

#define BG_COMMON_IOCTL_READ_RANDOM_CMD 0xFFAA
// random数据长度
#define DEVICE_RANDOM_LENGTH 32


#endif