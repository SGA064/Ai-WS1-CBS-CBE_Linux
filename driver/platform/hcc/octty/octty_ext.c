/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: octty extern functions
 */
/* 头文件包含 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>

#include <securec.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "octty.h"

#define GPIO_SYSFS_VALUE_HIGH "1"
#define GPIO_SYSFS_VALUE_LOW  "0"
#define GPIO_SYSFS_DIR_IN     "in"
#define GPIO_SYSFS_DIR_OUT    "out"
#define OCTTY_DELAY_50MS 50000
#define OCTTY_DELAY_1S 1000000

#define UART_MUX_REG_BASE 0x120c0000
#define UART_MUX_REG 0x120c0010
#define UART_MUX_REG_OFFSET (UART_MUX_REG - UART_MUX_REG_BASE)
#define UART_MUX_REG_LEN 0x100
#define UART_REG_MUX_MASK 0xfffffff0
#define CFG_GPIO_STATE 0x2
#define CFG_UART_STATE 0x4
#define SERIAL_OUT_BUFF_SIZE 128
#define OCTTY_UART_DEFAULT_BAUDRATE 1000000
#define OCTTY_CHIP_POWERON_DEFAULT_GPIO 40
#define OCTTY_UART_RX_DEFAULT_GPIO 12

#define UART_TTY_DEV_NAME "/dev/ttyAMA2"
static int g_detect_uart_fd = 0;
static uint32_t g_uart_baud_rate = OCTTY_UART_DEFAULT_BAUDRATE;
static int g_power_on_gpio = OCTTY_CHIP_POWERON_DEFAULT_GPIO;
static int g_uart_rx_gpio = OCTTY_UART_RX_DEFAULT_GPIO;

static int gpio_export(int gpio_index)
{
    char buf[SERIAL_OUT_BUFF_SIZE];
    int export_fd, gpio_fd, ret;

    if (sprintf_s(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", gpio_index) < 0) {
        return OCTTY_ERRCODE_FAIL;
    }
    gpio_fd = open(buf, O_WRONLY);
    if (gpio_fd >= 0) {
        return OCTTY_ERRCODE_SUCC;
    }

    export_fd = open("/sys/class/gpio/export", O_WRONLY);
    if (export_fd < 0) {
        close(gpio_fd);
        return OCTTY_ERRCODE_FAIL;
    }
    if (sprintf_s(buf, sizeof(buf), "%d", gpio_index) < 0) {
        ret = OCTTY_ERRCODE_FAIL;
        goto export_return;
    }
    ret = write(export_fd, buf, strlen(buf));
    if (ret < 0) {
        ret = OCTTY_ERRCODE_FAIL;
        goto export_return;
    }
    ret = OCTTY_ERRCODE_SUCC;
export_return:
    close(export_fd);
    close(gpio_fd);
    return ret;
}

static void gpio_unexport(int gpio_index)
{
    char buf[SERIAL_OUT_BUFF_SIZE];
    int export_fd, ret;
    export_fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (export_fd < 0) {
        return;
    }
    if (sprintf_s(buf, sizeof(buf), "%d", gpio_index) < 0) {
        close(export_fd);
        OCTTY_ERR("Unxport gpio [%d] failed\r\n", gpio_index);
        return;
    }
    write(export_fd, buf, strlen(buf));
    close(export_fd);
}

static int change_dir(int gpio_index, const char *dir)
{
    char str[SERIAL_OUT_BUFF_SIZE] = {0};
    char file_name[SERIAL_OUT_BUFF_SIZE] = {0};
    int ret;
    if (gpio_export(gpio_index) != OCTTY_ERRCODE_SUCC) {
        OCTTY_ERR("Export gpio [%d] failed\r\n", gpio_index);
        return OCTTY_ERRCODE_FAIL;
    }
    if (sprintf_s(str, sizeof(str), "%d", gpio_index) < 0) {
        return OCTTY_ERRCODE_FAIL;
    }
    if (sprintf_s(file_name, sizeof(file_name), "/sys/class/gpio/gpio%s/direction", str) < 0) {
        return OCTTY_ERRCODE_FAIL;
    }
    int fd = open(file_name, O_WRONLY);
    if (fd < 0) {
        OCTTY_ERR("change_dir fd null\r\n");
        return OCTTY_ERRCODE_FAIL;
    }
    ret = write(fd, dir, strlen(dir));
    if (ret != strlen(dir)) {
        close(fd);
        gpio_unexport(gpio_index);
        OCTTY_ERR("change_dir fail:[%s]\r\n", strerror(errno));
        return OCTTY_ERRCODE_FAIL;
    }
    close(fd);
    gpio_unexport(gpio_index);
    return OCTTY_ERRCODE_SUCC;
}

static int change_val(int gpio_index, const char *val)
{
    char str[10] = {0};
    char file_name[100] = {0};
    if (gpio_export(gpio_index) != OCTTY_ERRCODE_SUCC) {
        OCTTY_ERR("Export gpio [%d] failed\r\n", gpio_index);
        return OCTTY_ERRCODE_FAIL;
    }

    if (sprintf_s(str, sizeof(str), "%d", gpio_index) < 0) {
        return OCTTY_ERRCODE_FAIL;
    }
    if (sprintf_s(file_name, sizeof(file_name), "/sys/class/gpio/gpio%s/value", str) < 0) {
        return OCTTY_ERRCODE_FAIL;
    }
    int fd = open(file_name, O_WRONLY);
    if (fd < 0) {
        OCTTY_ERR("change_dir fd null\r\n");
        return OCTTY_ERRCODE_FAIL;
    }
    if (write(fd, val, strlen(val)) != strlen(val)) {
        OCTTY_ERR("change_val fail:[%s]\r\n", strerror(errno));
        return OCTTY_ERRCODE_FAIL;
    }
    close(fd);
    gpio_unexport(gpio_index);
    return OCTTY_ERRCODE_SUCC;
}

#define OCTTY_REG_MMAP_SIZE 0x104
#define OCTTY_UINT32_BYTE_SIZE 4
static uint32_t reg_read32(uint32_t reg_addr_base, uint32_t reg_addr_offset)
{
    int fd;
    void *map_reg_addr;
    uint32_t result;
    unsigned int *map_reg_addr_i;
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        OCTTY_ERR("reg_read32 open /dev/mem fail, fd = %08x\n", fd);
        return OCTTY_ERRCODE_FAIL;
    }
    map_reg_addr = mmap(0, OCTTY_REG_MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, reg_addr_base);
    if (map_reg_addr == MAP_FAILED) {
        close(fd);
        OCTTY_ERR("mmap Error.\r\n");
        return OCTTY_ERRCODE_FAIL;
    }
    map_reg_addr_i = (unsigned int *)map_reg_addr;
    result = *(volatile uint32_t *)(map_reg_addr_i + reg_addr_offset / OCTTY_UINT32_BYTE_SIZE);
    munmap(map_reg_addr, OCTTY_REG_MMAP_SIZE);
    close(fd);
    return result;
}

static uint32_t reg_write32(uint32_t reg_addr_base, uint32_t reg_addr_offset, uint32_t reg_val)
{
    int fd;
    void *map_reg_addr;
    uint32_t result;
    unsigned int *map_reg_addr_i;
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        OCTTY_ERR("reg_write32 open /dev/mem fail, fd = %08x\n", fd);
        return OCTTY_ERRCODE_FAIL;
    }
    map_reg_addr = mmap(0, OCTTY_REG_MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, reg_addr_base);
    if (map_reg_addr == MAP_FAILED) {
        close(fd);
        OCTTY_ERR("mmap Error\r\n");
        return OCTTY_ERRCODE_FAIL;
    }
    map_reg_addr_i = (unsigned int *)map_reg_addr;
    *(volatile uint32_t *)(map_reg_addr_i + reg_addr_offset / OCTTY_UINT32_BYTE_SIZE) = reg_val;
    munmap(map_reg_addr, OCTTY_REG_MMAP_SIZE);
    close(fd);
    return OCTTY_ERRCODE_SUCC;
}

static void ws73_board_power_off_gpio(int gpio_power_on)
{
    change_dir(gpio_power_on, GPIO_SYSFS_DIR_OUT);
    usleep(OCTTY_DELAY_50MS);
    change_val(gpio_power_on, GPIO_SYSFS_VALUE_LOW);
    usleep(OCTTY_DELAY_50MS);
}

static void ws73_board_power_on_gpio(int gpio_power_on)
{
    change_dir(gpio_power_on, GPIO_SYSFS_DIR_OUT);
    usleep(OCTTY_DELAY_50MS);
    change_val(gpio_power_on, GPIO_SYSFS_VALUE_HIGH);
    usleep(OCTTY_DELAY_50MS);
}

static void gpio_state_change_uart2cfg(void)
{
    uint32_t reg_val;

    reg_val = reg_read32(UART_MUX_REG_BASE, UART_MUX_REG_OFFSET);
    reg_val = reg_val & UART_REG_MUX_MASK;
    reg_val = reg_val | CFG_GPIO_STATE;
    if (reg_write32(UART_MUX_REG_BASE, UART_MUX_REG_OFFSET, reg_val) != 0) {
        OCTTY_ERR("gpio_state_change_uart2cfg write reg failed !!\r\n");
        return;
    }
    usleep(OCTTY_DELAY_50MS);
    change_dir(g_uart_rx_gpio, GPIO_SYSFS_DIR_OUT);
    usleep(OCTTY_DELAY_50MS);
    change_val(g_uart_rx_gpio, GPIO_SYSFS_VALUE_LOW);
    usleep(OCTTY_DELAY_50MS);
}

static void gpio_state_change_cfg2uart(void)
{
    uint32_t reg_val;

    reg_val = reg_read32(UART_MUX_REG_BASE, UART_MUX_REG_OFFSET);
    reg_val = reg_val & UART_REG_MUX_MASK;
    reg_val = reg_val | CFG_UART_STATE;
    if (reg_write32(UART_MUX_REG_BASE, UART_MUX_REG_OFFSET, reg_val) != 0) {
        OCTTY_ERR("gpio_state_change_cfg2uart write reg failed !!\r\n");
        return;
    }
    usleep(OCTTY_DELAY_50MS);
}

/* 通过拉GPIO进行device侧复位 */
static void ws73_board_power_reset_gpio(uint32_t gpio_power_on)
{
    /* 1. 下电 */
    ws73_board_power_off_gpio(gpio_power_on);
    /* 2. 配置GPIO */
    gpio_state_change_uart2cfg();
    /* 3. 上电 */
    ws73_board_power_on_gpio(gpio_power_on);
    /* 4. 配置GPIO */
    gpio_state_change_cfg2uart();
}

static void octty_deconfig_uart(void)
{
    if (g_detect_uart_fd < 0) {
        return;
    }

    close(g_detect_uart_fd);
    g_detect_uart_fd = -1;
}

static void octty_config_uart(void)
{
    /* ===octty_config_uart step 1=== */
    if (g_detect_uart_fd != 0) {
        octty_deconfig_uart();
    }
    g_detect_uart_fd = open(UART_TTY_DEV_NAME, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
    if (g_detect_uart_fd < 0) {
        OCTTY_ERR("\n  Error! in Opening %s ,ret:%s \r\n", UART_TTY_DEV_NAME, strerror(errno));
        return;
    }
    /* ===octty_config_uart step 2=== */
    struct termios uart_cfg;
    struct termios2 uart_cfg2;
    tcgetattr(g_detect_uart_fd, &uart_cfg);
    cfmakeraw(&uart_cfg);
    /* Setting the Baud rate */
    cfsetispeed(&uart_cfg, B1000000);
    cfsetospeed(&uart_cfg, B1000000);

    uart_cfg.c_cflag |= CLOCAL;
    uart_cfg.c_cflag |= CRTSCTS;

    uart_cfg.c_cc[VTIME] = 0;
    uart_cfg.c_cc[VMIN] = 1;
    tcsetattr(g_detect_uart_fd, TCSANOW, &uart_cfg);
    tcflush(g_detect_uart_fd, TCIOFLUSH);

    ioctl(g_detect_uart_fd, TCGETS2, &uart_cfg2);
    uart_cfg2.c_cflag &= ~CBAUD;
    uart_cfg2.c_cflag |= BOTHER;
    uart_cfg2.c_ispeed = g_uart_baud_rate;
    uart_cfg2.c_ospeed = g_uart_baud_rate;
    ioctl(g_detect_uart_fd, TCSETS2, &uart_cfg2);
}
#define PRI_LEN 8
static void buf_print(char *str, unsigned char *buf, int len)
{
    int i = 0;
    int j = 0;

    if (str == NULL || buf == NULL) {
        return;
    }

    OCTTY_DBG("******** %s print begin len=%d********\n", str, len);
    while (i < len) {
        if ((len - i) < PRI_LEN) {
            for (; i < len; i++) {
                OCTTY_DBG("0x%02X ", buf[i]);
            }
        } else {
            for (j = 0; j < PRI_LEN; j++) {
                OCTTY_DBG("0x%02X ", buf[i + j]);
            }
            i += PRI_LEN;
        }
        OCTTY_DBG("\n");
    }
    OCTTY_DBG("******** %s print end ********\n", str);
    return;
}

static int uart_send(unsigned char *buf, int len)
{
    int write_len = -1;
    if (g_detect_uart_fd < 0) {
        OCTTY_ERR("uart_send fd null\r\n");
        return OCTTY_ERRCODE_FAIL;
    }

    write_len = write(g_detect_uart_fd, buf, len);
    if (write_len != len) {
        OCTTY_ERR("uart_send error : [%s]\r\n", strerror(errno));
        return OCTTY_ERRCODE_FAIL;
    }
    return OCTTY_ERRCODE_SUCC;
}

static int uart_read(unsigned char *buf, int len)
{
    int rcv_len = 0;

    if (g_detect_uart_fd < 0) {
        OCTTY_ERR("uart_read fd null\r\n");
        return OCTTY_ERRCODE_FAIL;
    }

    rcv_len = read(g_detect_uart_fd, buf, len);
    if ((rcv_len < 0) && (errno != EAGAIN)) {
        OCTTY_ERR("uart_read error : [%s]\r\n", strerror(errno));
        return OCTTY_ERRCODE_FAIL;
    }
    if (rcv_len == 0) {
        OCTTY_ERR("uart_read null \r\n");
        return OCTTY_ERRCODE_FAIL;
    }
    return rcv_len;
}
#define OCTTY_PARAM_INDEX_CHIP 1
#define OCTTY_PARAM_INDEX_UARTBAUD 2
#define OCTTY_PARAM_INDEX_UARTRX 4
#define OCTTY_PARAM_INDEX_POWERON 5
#define OCTTY_PARAM_CNT 6
static uint32_t param_check(int argc, char *argv[])
{
    if (argc < OCTTY_PARAM_CNT) {
        OCTTY_ERR("octty ws73 check, param invalid !\r\n");
        return OCTTY_ERRCODE_FAIL;
    }
    if (strcmp(argv[OCTTY_PARAM_INDEX_CHIP], "ws73") != 0) {
        OCTTY_ERR("octty chip not support:[%s]\r\n", argv[1]);
        return OCTTY_ERRCODE_FAIL;
    }
    g_uart_baud_rate = (uint32_t)atoi(argv[OCTTY_PARAM_INDEX_UARTBAUD]);
    if (g_uart_baud_rate < 0) {
        OCTTY_ERR("octty uart baudrate invalid, use default\r\n");
        g_uart_baud_rate = OCTTY_UART_DEFAULT_BAUDRATE;
    }
    g_uart_rx_gpio = atoi(argv[OCTTY_PARAM_INDEX_UARTRX]);
    if (g_uart_rx_gpio < 0) {
        OCTTY_ERR("octty uart rx param invalid, use default\r\n");
        g_uart_rx_gpio = OCTTY_UART_RX_DEFAULT_GPIO;
    }
    g_power_on_gpio = atoi(argv[OCTTY_PARAM_INDEX_POWERON]);
    if (g_power_on_gpio < 0) {
        OCTTY_ERR("octty power on gpio invalid, use default\r\n");
        g_power_on_gpio = OCTTY_CHIP_POWERON_DEFAULT_GPIO;
    }
    return OCTTY_ERRCODE_SUCC;
}

// octty [chip_type_to_detect] [bardrate] [xtal freq] [gpio_uart_rx] [gpio_power_on]
// octty ws73 1000000 40M 0 40
#define VER_CMD_KEYWORD  "VERSION"
#define COMPART_KEYWORD  ((char)' ')
#define VER_EXPECT_VALUE "BOOTVER001"
int octty_chip_detect(int argc, char *argv[])
{
    int i;
    int ret;
    char send_data[SERIAL_OUT_BUFF_SIZE];
    unsigned char test_str[SERIAL_OUT_BUFF_SIZE];
    if (param_check(argc, argv) != OCTTY_ERRCODE_SUCC) {
        ret = OCTTY_ERRCODE_FAIL;
        goto detect_return;
    }

    ws73_board_power_reset_gpio(g_power_on_gpio);
    octty_config_uart();
    usleep(OCTTY_DELAY_50MS);
    if (sprintf_s(send_data, sizeof(send_data), "%s%c", VER_CMD_KEYWORD, COMPART_KEYWORD) < 0) {
        octty_deconfig_uart();
        ret = OCTTY_ERRCODE_FAIL;
        goto detect_return;
    }
    uart_send(send_data, strlen(send_data));
    usleep(OCTTY_DELAY_50MS);
    memset_s(test_str, sizeof(test_str), 0, sizeof(test_str));
    uart_read(test_str, SERIAL_OUT_BUFF_SIZE);
    octty_deconfig_uart();
    ret = memcmp(test_str, VER_EXPECT_VALUE, strlen(VER_EXPECT_VALUE));
detect_return:
    OCTTY_ERR("octty chip detect test done\r\n");
    return ret;
}