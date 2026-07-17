/*
 * Copyright (c) @CompanyNameMagicTag 2023-2023. All rights reserved.
 */
#include <stdio.h>
#include <unistd.h>
#include "securec.h"
#include "msg_queue.h"

int dft_ctrl(const char* ctrl_name)
{
    int msgqueue_id = test_suite_get_queue();
    char buf[SIZE];
    if (strncpy_s(buf, SIZE, START_MSG, strlen(START_MSG) + 1) != EOK) {
        perror("strncpy_s error");
    }
    test_suite_send_msg(msgqueue_id, CLIENT_TYPE, buf);  // 发送一条启动消息
    printf("waiting server start...\n");
    int ret = test_suite_recv_msg(msgqueue_id, SERVER_TYPE, buf, SIZE); // 等待server响应
    if (ret == 0 && strncasecmp(buf, START_MSG, 5) == 0) { // start len 5
        printf("server started\n");
        while (1) {
            printf("%s>", ctrl_name);
            (void)fflush(stdout);
            if (memset_s(buf, sizeof(buf), '\0', sizeof(buf)) != EOK) {
                perror("memset_s error");
            }
            (void)fgets(buf, SIZE - 1, stdin);
            if (strncasecmp(buf, "Exit", 4) == 0) { // quit len 4
                printf("Exit\n");
                return 0;
            }
            test_suite_send_msg(msgqueue_id, CLIENT_TYPE, buf);  // 经buf里面的消息发送给server
            if (strncasecmp(buf, "Quit", 4) == 0) { // quit len 4
                printf("Quit\n");
                return 0;
            }
            usleep(50 * 1000); // 50 * 1000 = 50ms
            if (memset_s(buf, sizeof(buf), '\0', sizeof(buf)) != EOK) { // buf保存消费掉的消息
                perror("memset_s error");
            }
        }
    }
    return 0;
}