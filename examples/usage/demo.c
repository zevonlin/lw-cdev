/*
 * Copyright (c) 2026-2036
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-04-24     zevonlin     zevonlin
 */
#include "lw_cdev.h"

static int8_t demo_cdev_init(lw_cdev_cfg_t cfg);
static int8_t demo_cdev_open(lw_cdev_cfg_t cfg, uint16_t open_flag);
static int8_t demo_cdev_close(lw_cdev_cfg_t cfg);
static ptrdiff_t demo_cdev_read(lw_cdev_cfg_t cfg, size_t pos, char* buf, size_t count, size_t timeout);
static ptrdiff_t demo_cdev_write(lw_cdev_cfg_t cfg, size_t pos, const char* buf, size_t count, size_t timeout);
static int8_t demo_cdev_ioctl(lw_cdev_cfg_t cfg, const uint8_t cmd, void* arg);

static CRITICAL_SECTION demo_core_lock;
static HANDLE demo_dev_lock;
static lw_cdev_t demo_cdev;
static char demo_buf[256] = {0};

static lw_cdev_adapter demo_adapter = {
    .init = demo_cdev_init,
    .open = demo_cdev_open,
    .close = demo_cdev_close,
    .read = demo_cdev_read,
    .write = demo_cdev_write,
    .ioctl = demo_cdev_ioctl,
};

static int8_t demo_cdev_init(lw_cdev_cfg_t cfg)
{
    printf("[demo dev adpt] init success\n");
    return 0;
}

static int8_t demo_cdev_open(lw_cdev_cfg_t cfg, uint16_t open_flag)
{
    cfg->flag = open_flag;
    printf("[demo dev adpt] open success (flag:0x%04X)\n", open_flag);
    return 0;
}

static int8_t demo_cdev_close(lw_cdev_cfg_t cfg)
{
    printf("[demo dev adpt] close success\n");
    return 0;
}

static ptrdiff_t demo_cdev_read(lw_cdev_cfg_t cfg, size_t pos, char* buf, size_t count, size_t timeout)
{
    if (buf == NULL || count == 0) {
        cfg->error |= LW_CDEV_ERR_PARAM;
        return -1;
    }
    memcpy(buf, demo_buf + pos, count);
    printf("[demo dev adpt] read done\n");
    return count;
}

static ptrdiff_t demo_cdev_write(lw_cdev_cfg_t cfg, size_t pos, const char* buf, size_t count, size_t timeout)
{
    if (buf == NULL || count == 0) {
        cfg->error |= LW_CDEV_ERR_PARAM;
        return -1;
    }

    if (strlen(buf) == count) {
        printf("[demo dev adpt] write %zu bytes: %.*s\n", count, (int)count, buf);
    }
    else {
        printf("[demo dev adpt] inconsistency of data ! count = %d,len = %d\n", count, strlen(buf));
    }

    memset(demo_buf, 0, sizeof(char) * 256);
    memcpy(demo_buf, buf, count + 1);

    return count;
}

static int8_t demo_cdev_ioctl(lw_cdev_cfg_t cfg, const uint8_t cmd, void* arg)
{
    switch (cmd) {
    case LW_CDEV_CMD_HW_INIT:
        printf("[demo dev adpt] ioctl: LW_CDEV_CMD_HW_INIT\n");
        return 0;
    case LW_CDEV_CMD_SET_CONFIG:
        printf("[demo dev adpt] ioctl: LW_CDEV_CMD_SET_CONFIG\n");
        return 0;
    case LW_CDEV_CMD_GET_CONFIG:
        printf("[demo dev adpt] ioctl: LW_CDEV_CMD_GET_CONFIG\n");
        return 0;
    default:
        cfg->error |= LW_CDEV_ERR_UNSUPPORT;
        return -1;
    }
}

void lock_init(void) {
    InitializeCriticalSectionAndSpinCount(&demo_core_lock, 4000);
    demo_dev_lock = CreateMutex(NULL, FALSE, NULL);
}

void demo_task(void)
{
    printf("===== Executing the routine begins =====\n\n");
    int8_t ret;
    ptrdiff_t len;
    lock_init();

    ret = lw_cdev_manager_init(&demo_core_lock);
    if (ret < 0) {
        printf("[demo dev task] lw_cdev_manager_init fail: %d\n", ret);
        return;
    }
    else {
        printf("[demo dev task] lw_cdev_manager_init success\n");
    }


    demo_cdev = lw_cdev_create(LW_CDEV_TYPE_CHAR, NULL, demo_dev_lock);
    if (demo_cdev == NULL) {
        printf("[demo dev task] create demo dev fail\n");
        return;
    }
    else {
        printf("[demo dev task] create demo dev success\n");
    }

    static char* demo_name = "demo_cdev";
    ret = lw_cdev_register(demo_cdev, demo_name, &demo_adapter);
    if (ret < 0) {
        printf("[demo dev task] register global demo dev fail: %d\n", ret);
        return;
    }
    else {
        printf("[demo dev task] register global demo dev success (id=%d)\n", lw_cdev_get_id(demo_cdev));
    }

    lw_cdev_t find_dev = lw_cdev_find("demo_cdev");
    if (find_dev == NULL || find_dev != demo_cdev) {
        printf("[demo dev task] find demo dev fail\n");
        return;
    } 
    else {
        printf("[demo dev task] find demo dev success\n");
    }

    ret = lw_cdev_open(find_dev, LW_CDEV_FLAG_RDWR);
    if (ret < 0) {
        printf("[demo dev task] open demo dev fail: %d\n", ret);
        return;
    }
    else {
        printf("[demo dev task] open demo dev success\n");
    }

    char *write_buf = "some unimportant content";
    len = lw_cdev_write(find_dev, 0, write_buf, strlen(write_buf), 1000);
    if (len < 0) {
        printf("[demo dev task] write demo dev fail: %zd\n", len);
        return;
    }
    else {
        printf("[demo dev task] write demo dev success! size = %d,return_size = %d\n", strlen(write_buf), len);
    }

    ret = lw_cdev_ioctl(find_dev, LW_CDEV_CMD_GET_CONFIG, NULL);
    if (ret < 0) {
        printf("[demo dev task] ioctl demo dev fail: %d\n", ret);
        return;
    }
    else {
        printf("[demo dev task] ioctl demo dev success\n");
    }

    char read_buf[64] = { 0 };
    len = lw_cdev_read(find_dev, 0, read_buf, strlen(write_buf), 1000);
    if (len < 0) {
        printf("[demo dev task] read demo dev fail: %zd\n", len);
        return;
    }
    else {
        printf("[demo dev task] read demo dev success! read %zu bytes: %.*s\n", strlen(write_buf), (int)strlen(write_buf), read_buf);
    }

    ret = lw_cdev_close(find_dev);
    if (ret < 0) {
        printf("[demo dev task] close demo dev fail: %d\n", ret);
        return;
    }
    else {
        printf("[demo dev task] close demo dev success!\n");
    }

    ret = lw_cdev_unregister("demo_cdev");
    if (ret < 0) {
        printf("[demo dev task] unregister demo dev fail,error :%d!\n", ret);
        return;
    }
    else {
        lw_cdev_t dev_unregister = lw_cdev_find("demo_cdev");
        if (dev_unregister == NULL) {
            printf("[demo dev task] unregister demo dev success!\n");
        }
        else {
            printf("[demo dev task] unregister demo dev fail,still mounted!\n");
        }
    }
    printf("\n===== The routine has been executed. =====\n\n");
}

