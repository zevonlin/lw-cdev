#ifndef _LW_CDEV_H_
#define _LW_CDEV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lw_cdev_def.h"

typedef struct lw_cdev *lw_cdev_t;
typedef struct lw_cdev_cfg *lw_cdev_cfg_t;


typedef struct {
    /* 设备初始化*/
    int8_t   (*init )(lw_cdev_cfg_t cfg);
    int8_t   (*open )(lw_cdev_cfg_t cfg,uint16_t open_flag);
    int8_t   (*close)(lw_cdev_cfg_t cfg);
    ptrdiff_t(*read )(lw_cdev_cfg_t cfg,size_t pos,char *buf, size_t count, size_t timeout);
    ptrdiff_t(*write)(lw_cdev_cfg_t cfg,size_t pos,const char *buf, size_t count, size_t timeout);
    /**
     * @brief 该命令允许扩展，一般由用户数据自定义命令与拓展
     * @param cmd 默认使用LW_CDEV_CMD_宏定义，允许自定义扩展，扩展由LW_CDEV_CMD_DEF_MAX起始
     * @param arg 根据cmd功能自适应，由用户适配
     * 
     */
    int8_t   (*ioctl)(lw_cdev_cfg_t cfg,const uint8_t cmd, void *arg);
} lw_cdev_adapter;


struct lw_cdev_cfg{
    void*               private_data;       /**< 驱动私有数据指针（默认必选） */
#if lw_cdev_user
    void*               user_data;          /**< 用户数据指针（可选） */
#endif

    uint8_t             error;              /**< 位保存异常状态,由LW_CDEV_ERR_宏定义*/
    uint8_t             state;              /**< 运行状态,由LW_CDEV_STATE_宏定义*/
    uint16_t            flag;               /**< 打开方式,由LW_CDEV_FLAG_宏定义 */
};

/**
 * @brief 初始化字符设备管理器
 * @note  在使用字符设备管理器时必须使用
 * @param core_lock 当lw_cdev_rtos为1时，传入外部申请的自旋锁(优先)或互斥锁地址，否则传入NULL
 * @return int8_t ret = 0：成功
 *                ret <= 0 异常
 */
int8_t lw_cdev_manager_init(void *core_lock);

/**
 * @brief 动态创建设备
 * 
 * @note  该操作会清空设备内部所有携带数据
 * @param cdev 设备句柄
 * @param type 表示设备对象的类型
 * @param config_data 配置数据 
 * @param lock 当lw_cdev_rtos为1时，传入互斥锁或自旋锁地址，否则传入NULL
 * @return lw_cdev_t 获取设备句柄 查找到对应设备将返回相应的设备句柄
 *         NULL      没有找到相应的设备对象
 */
lw_cdev_t lw_cdev_create(uint8_t type,void *config_data,void *lock);

/**
 * @brief 静态创建设备
 * 
 * @note  该操作会清空设备内部所有携带数据
 * @param cdev 设备句柄
 * @param type 表示设备对象的类型
 * @param config_data 配置数据
 * @param lock 当lw_cdev_rtos为1时，传入互斥锁或自旋锁地址，否则传入NULL
 * @return int8_t ret = 0：创建设备成功
 *                ret <= 0 异常
 */
int8_t lw_cdev_static_create(lw_cdev_t cdev,uint8_t type,void *config_data,void *lock);

/**
 * @brief 注册设备
 * 
 * @note  该设备将直接挂载在设备总线上，总线最大允许128个设备
 * @param cdev 设备对象指针,仅允许静态申请
 * @param name 设备名称,仅允许静态存放
 * @param adpt 设备接口,仅允许静态存放
 * @return int8_t ret > 0：挂载期间唯一设备号
 *                ret <= 0 异常
 */
int8_t lw_cdev_register(lw_cdev_t cdev, const char* name, lw_cdev_adapter* adpt);

/**
 * @brief 卸载设备
 * 
 * @note  该操作将会将设备从设备总线上卸载，如果是动态创建的内存，将会被释放
 * @param name 设备名称,仅允许静态存放
 * @return int8_t ret = 0：卸载成功
 *                ret <= 0 异常
 */
int8_t lw_cdev_unregister(const char *name);

/**
 * @brief 查找设备
 * @note  返回句柄，查找时间复杂度将降至O(1) 
 * 
 * @param name 设备名称
 * @return lw_cdev_t 获取设备句柄 查找到对应设备将返回相应的设备句柄
 *         NULL      没有找到相应的设备对象
 */
lw_cdev_t lw_cdev_find(const char *name);

/**
 * @brief 初始化设备
 * @note 若lw_cdev_rtos = lw_cdev_auto_locking = 1,则不允许内部嵌套其他设备控制函数
 * 
 * @param cdev 设备句柄
 * @return int8_t ret = 0：成功
 *                ret <= 0 异常
 */
int8_t lw_cdev_init(lw_cdev_t cdev);


 /**
  * @brief 打开设备
  *
  * @param cdev        字符设备句柄（必须由 lw_cdev_create() 创建）
  * @param open_flag   打开标志位，支持以下值：
  *                     - LW_CDEV_FLAG_RDONLY: 只读
  *                     - LW_CDEV_FLAG_WRONLY: 只写
  *                     - LW_CDEV_FLAG_RDWR:   读写
  *                     - LW_CDEV_FLAG_STREAM: 流模式
  *
 * @return int8_t ret = 0：成功
 *                ret <= 0 异常
  *
  * @note 1. 线程安全：内部会自动加锁，多线程可安全调用
  *       2. 首次打开会自动调用设备初始化函数（adpt->init）
  *       3. 调用后必须配对调用 lw_cdev_close()，否则会资源泄漏
  *       4. 若lw_cdev_rtos = lw_cdev_auto_locking = 1,则不允许内部嵌套其他设备控制函数
  */
int8_t lw_cdev_open(lw_cdev_t cdev,uint16_t open_flag);

/**
 * @brief 关闭设备
 * @note 若lw_cdev_rtos = lw_cdev_auto_locking = 1,则不允许内部嵌套其他设备控制函数
 * 
 * @param cdev 设备句柄
 * @return int8_t ret = 0：成功
 *                ret <= 0 异常
 */
int8_t lw_cdev_close(lw_cdev_t cdev);

/**
 * @brief 读取数据
 * @note 若lw_cdev_rtos = lw_cdev_auto_locking = 1,则不允许内部嵌套其他设备控制函数
 * 
 * @param cdev 设备句柄
 * @param pos 写入起始位置：块设备为起始块号，字符设备为字节偏移或自定义内容
 * @param buf 读取数据缓存位置
 * @param count 读取数据大小
 * @param timeout 超时时间
 * @return ptrdiff_t ret > 0：成功，返回读取长度
 *                ret <= 0 异常
 */
ptrdiff_t lw_cdev_read(lw_cdev_t cdev, size_t pos,char *buf, size_t count, size_t timeout);

/**
 * @brief 写入数据
 * @note 若lw_cdev_rtos = lw_cdev_auto_locking = 1,则不允许内部嵌套其他设备控制函数
 * 
 * @param cdev 设备句柄
 * @param pos 写入起始位置：块设备为起始块号，字符设备为字节偏移或自定义内容
 * @param buf 写入数据缓存位置
 * @param count 写入数据大小
 * @param timeout 超时时间
 * @return ptrdiff_t ret > 0：成功，返回写入长度
 *                ret <= 0 异常
 */
ptrdiff_t lw_cdev_write(lw_cdev_t cdev, size_t pos, const char *buf, size_t count, size_t timeout);

/**
 * @brief 设备命令控制
 * @note 若lw_cdev_rtos = lw_cdev_auto_locking = 1,则不允许内部嵌套其他设备控制函数
 * 
 * @param cdev 设备句柄
 * @param cmd 设备允许的命令字
 * @param arg 命令字对应的携带数据，若无则填写NULL
 * @return int8_t ret = 0：成功
 *                ret <= 0 异常
 */
int8_t lw_cdev_ioctl(lw_cdev_t cdev, const uint8_t cmd, void *arg);

/**
 * @brief 获取设备ID
 *
 * @param cdev 设备句柄
 * @return int8_t ret >= 0：ID号
 *                ret < 0 异常
 */
int8_t lw_cdev_get_id(lw_cdev_t cdev);

/**
 * @brief 获取设备类型
 *
 * @param cdev 设备句柄
 * @return int8_t ret >= 0：类型
 *                ret < 0 异常
 */
lw_cdev_type_e lw_cdev_get_type(lw_cdev_t cdev);

#ifdef __cplusplus
}
#endif

#endif

/* _LW_CDEV_H */
