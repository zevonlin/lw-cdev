#ifndef _LW_CDEV_DEF_H_
#define _LW_CDEV_DEF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/**
 * @brief 是否启用RTOS支持（必须为1，否则auto_locking无意义）
 * @note 启用后需适配下方的自旋锁/互斥锁宏实现
 */
#define lw_cdev_rtos            1

 /**
  * @brief 设备操作自动加锁开关（用户指定为1）
  * @note 启用后，init/open/read/write/close/ioctl会自动加解锁，无需用户手动处理
  *       依赖 lw_cdev_rtos = 1 才能生效
  */
#define lw_cdev_auto_locking    1

  /**
   * @brief 是否启用用户数据字段（struct lw_cdev_cfg->user_data）
   * @note 0=关闭（节省内存），1=开启（支持用户自定义数据挂载）
   */
#define lw_cdev_user            0

   /**
    * @brief 是否启用断言检查
    * @note 1=开启（调试阶段），0=关闭（发布阶段，节省性能）
    */
#define lw_cdev_assertion       1

#define lw_cdev_malloc(size)       malloc(size)
#define lw_cdev_free(mem)          free(mem)
#define lw_cdev_memset(s,c,count)  memset(s,c,count)
#define lw_cdev_strncmp(s1,s2,n)   strncmp(s1,s2,n)
#define lw_cdev_strlen(s)          strlen(s)

#if lw_cdev_rtos
#define lw_cdev_spin_lock(lock)    ((void)0)
#define lw_cdev_spin_unlock(lock)  ((void)0)
#else
#define lw_cdev_spin_lock(lock)    ((void)0)
#define lw_cdev_spin_unlock(lock)  ((void)0)
#endif

#if lw_cdev_rtos && lw_cdev_auto_locking
#define lw_cdev_mutex_lock(lock)   ((void)0) 
#define lw_cdev_mutex_unlock(lock) ((void)0)
#else
#define lw_cdev_mutex_lock(lock)   ((void)0)
#define lw_cdev_mutex_unlock(lock) ((void)0)
#endif

#if lw_cdev_assertion
    #define cdev_assert(ex, ret)     \
        do {                         \
            if (!(ex)) {             \
                return ret;          \
            }                        \
        } while(0)
#else
    #define cdev_assert(ex, ret)    (void)(ex)
#endif


typedef enum {
    LW_CDEV_TYPE_ERROR = -1,
    LW_CDEV_TYPE_CHAR = 0,    // 字符设备
    LW_CDEV_TYPE_BLOCK,       // 块设备
    LW_CDEV_TYPE_BUS,         // 总线设备
    LW_CDEV_TYPE_PROTOCOL,    // 协议设备
    LW_CDEV_TYPE_UART,        // 串口硬件
    LW_CDEV_TYPE_SPI,         // SPI硬件
    LW_CDEV_TYPE_I2C,         // I2C硬件
    LW_CDEV_TYPE_ADC,         // ADC硬件
    LW_CDEV_TYPE_PIN,         // 引脚硬件
    LW_CDEV_TYPE_SENSOR,      // 传感器硬件
    LW_CDEV_TYPE_UNKNOWN,     // 未知
} lw_cdev_type_e;


/**
 * @brief 轻量字符设备 ioctl 控制命令枚举
 * @note 硬件/协议设备通用，用户自定义命令从 LW_CDEV_CMD_DEF_MAX 开始
 */
typedef enum {

    /**
     * @brief 硬件命令
     * @note  仅LW_CDEV_TYPE_PROTOCOL类型定义有效
     * 
     */
    LW_CDEV_CMD_HW_INIT    = 0, /**< 硬件初始化 */
    LW_CDEV_CMD_HW_ENABLE     , /**< 硬件使能 */
    LW_CDEV_CMD_HW_RESET      , /**< 硬件复位 */
    LW_CDEV_CMD_HW_SLEEP      , /**< 硬件休眠 */
    LW_CDEV_CMD_HW_WAKEUP     , /**< 硬件唤醒 */

    LW_CDEV_CMD_SUSPEND       , /**< 挂起设备（低功耗） */
    LW_CDEV_CMD_RESUME        , /**< 恢复设备 */

    /* 错误管理命令 */
    LW_CDEV_CMD_SET_ERR       , /**< 设置用户错误码 */
    LW_CDEV_CMD_GET_ERR       , /**< 获取用户错误码 */

    /* 设备功能配置 */
    LW_CDEV_CMD_SET_PRIORITY  , /**< 优先级配置 */
    LW_CDEV_CMD_CLEAR_RX      , /**< 清空接收缓存 */
    LW_CDEV_CMD_CLEAR_TX      , /**< 清空发送缓存 */

    /* 设备参数配置 */
    LW_CDEV_CMD_SET_CONFIG    , /**< 配置设备 */
    LW_CDEV_CMD_GET_CONFIG    , /**< 获取设备配置 */

    /* 用户私有数据 */
    LW_CDEV_CMD_SET_USER_DATA , /**< 配置用户数据 */
    LW_CDEV_CMD_GET_USER_DATA , /**< 获取用户数据 */

    LW_CDEV_CMD_DEF_MAX         /**< 用户扩展命令起始值 */
} lw_cdev_cmd_e;

/**
 * @brief 设备属性标志 (对应结构体: uint16_t flag)
 * @note 设备硬件属性/工作模式，支持按位 | 组合
 */
#define LW_CDEV_FLAG_DEACTIVATE      0x0000U    // 设备未初始化
#define LW_CDEV_FLAG_RDONLY          0x0001U    // 只读权限
#define LW_CDEV_FLAG_WRONLY          0x0002U    // 只写权限
#define LW_CDEV_FLAG_RDWR            0x0003U    // 读写权限（最常用）
#define LW_CDEV_FLAG_STREAM          0x0004U    // 流模式（串口/网络/协议设备专用）
#define LW_CDEV_FLAG_MASK            0x000fU

/**
 * @brief 运行状态标志 (对应结构体: uint16_t state)
 * @note 设备当前运行状态，互斥使用
 */
#define LW_CDEV_STATE_CLOSED         0x00U    // 设备已关闭
#define LW_CDEV_STATE_OPENED         0x01U    // 设备已打开
#define LW_CDEV_STATE_ACTIVATED      0x02U    // 设备已激活（工作中）
#define LW_CDEV_STATE_SUSPENDED      0x04U    // 设备已挂起（低功耗）

/**
 * @brief 设备错误标志位掩码 (按位存储，支持多错误同时标记)
 * @note 对应 struct lw_cdev->error (uint8_t)
 */
#define LW_CDEV_ERR_NONE            0x00U    /**< 无错误 */
#define LW_CDEV_ERR_FAULT           0x01U    /**< 硬件故障（初始化/复位/休眠失败）*/
#define LW_CDEV_ERR_READ_FAIL       0x02U    /**< 发送失败（缓存满/硬件无响应）*/
#define LW_CDEV_ERR_WRITE_FAIL      0x04U    /**< 写失败（超时/数据丢失）*/
#define LW_CDEV_ERR_PROTOCOL        0x08U    /**< 协议错误（帧格式/校验失败*/
#define LW_CDEV_ERR_PARAM           0x10U    /**< 参数错误（空指针/非法配置）*/
#define LW_CDEV_ERR_BUSY            0x20U    /**< 设备忙（正在传输，不可操作）*/
#define LW_CDEV_ERR_TIMEOUT         0x40U    /**< 操作超时（读写/硬件响应超时）*/
#define LW_CDEV_ERR_UNSUPPORT       0x80U    /**< 不支持的命令/功能（ioctl/配置无效）*/


#ifdef __cplusplus
}
#endif

#endif

/* _LW_CDEV_DEF_H */
