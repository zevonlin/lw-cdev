/**
 * @file lw_cdev.c
 * @author zevonlin@gmail.com
 * @brief 轻量级设备管理器
 * @version 0.1
 * @date 2026-04-21
 * 
 * @copyright Copyright (c) 2026-2036 zevonlin
 * 
 */

#include "lw_cdev.h"

#define LW_CDEV_MAGIC  0x5A5A1001U
/* 更改该数值虚同步更改ID池和查找方式*/
#define LW_CDEV_MAX_ID 127

/**
 * @note ：不使用哈希表：避免哈希冲突和RAM占用带来的麻烦
 *         不适用数组下标或静态链表：没必要
 */

/**
 * @brief 
 * @note ：该方案考虑了结构体对齐,最大总大小为44字节
 *         考虑了访问速度，默认4字节对齐（32bit）
 *         增加缓存命中率
 * 
 */
struct lw_cdev {
    const char* name;
    /* CPU 加载结构体首地址后，能以极小的偏移量快速获取 next 指针*/
    struct  lw_cdev* next;      /**< 链表 */
    lw_cdev_adapter* adpt;      /**< 接口 */
#if lw_cdev_rtos && lw_cdev_auto_locking
    void* lock;
#endif
    struct  lw_cdev_cfg cfg;

    int8_t(*rx_indicate)(lw_cdev_t dev, size_t size);
    int8_t(*tx_complete)(lw_cdev_t dev, void* buf);

    uint8_t id;                  /**< ID号,0-127,若超出需修改ID获取方式*/
    uint8_t type;                /**< 定义,由LW_CDEV_TYPE_枚举定义*/
    uint8_t allocation_mode;     /**< 分配模式，静态 = 0，动态 = 1 */
    uint8_t ref_count;           /**< 引用计数，每次使用Open将会+1*/

    uint32_t magic;              /**< 合法性校验*/
};

static lw_cdev_t cdev_list = NULL;
/* ID池*/
static uint32_t cdev_id_bitmap[4] = {0};
#if lw_cdev_rtos
/* 不直接使用cdev_list，避免首节点在动态申请被释放后成野指针*/
static void *cdev_list_lock = NULL;
#endif
/* 对比*/
#define _cdev_name_cmp(cur,cmp) lw_cdev_strncmp(cur, cmp, lw_cdev_strlen(cmp))
/* 检查ID是否被占用（O(1)位运算,超出范围（>127）视为“已占用”，避免越界访问*/
#define _cdev_is_id_used(id) (((cdev_id_bitmap[id / 32] & (1U << (id % 32)))) ? 1 : 0)
/* 标记ID为已占用*/
#define _cdev_set_id_used(id) (cdev_id_bitmap[((id) / 32)] |= (1U << ((id) % 32)))
/* 清除ID的占用标记（注销时复用）*/
#define _cdev_clear_id_used(id) (cdev_id_bitmap[((id) / 32)] &= ~(1U << ((id) % 32)))

/**
 * @brief 查找设备
 * 
 * @param name 设备名称
 * @return lw_cdev_t 设备句柄
 */
static lw_cdev_t _lw_cdev_find(const char *name)
{
    /* 此函数线程不安全*/
    lw_cdev_t curr = cdev_list;
    while (curr) {
        if (!_cdev_name_cmp(curr->name, name)) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

/**
 * @brief 获取可用ID
 * 
 * @return int8_t 可用ID号
 *         -1     ID全被占用
 */
static int8_t _lw_cdev_alloc_id(void)
{
    uint8_t target_id;
    /* 遍历查找最小ID*/
    for (target_id = 0; target_id <= LW_CDEV_MAX_ID; target_id++) {
        if (!_cdev_is_id_used(target_id)) {
            _cdev_set_id_used(target_id);
            return target_id;
        }
    }
    return -1; /* 128个ID全被占用*/
}

int8_t lw_cdev_manager_init(void *core_lock)
{
#if lw_cdev_rtos
    cdev_assert(core_lock != NULL,-1);
    cdev_list_lock = core_lock;
#endif
    return 0;
}

lw_cdev_t lw_cdev_create(uint8_t type,void *config_data,void *lock)
{
    /* 不涉及线程安全，因此是线程安全的*/
    cdev_assert((lw_cdev_rtos && lw_cdev_auto_locking) ? (lock != NULL) : 1, NULL);
    lw_cdev_t cdev = (lw_cdev_t)lw_cdev_malloc(sizeof(struct lw_cdev));
    if(cdev == NULL){
        return NULL;
    }

    lw_cdev_memset(cdev, 0x00, sizeof(struct lw_cdev));
    if(type >= LW_CDEV_TYPE_UNKNOWN) {
        cdev->cfg.error |= LW_CDEV_ERR_PARAM;
        cdev->type = LW_CDEV_TYPE_UNKNOWN;
    }
    else{
        cdev->type = type;
    }

#if lw_cdev_rtos && lw_cdev_auto_locking
    cdev->lock = lock;
#endif
    cdev->cfg.private_data = config_data;
    /* 标记为动态申请*/
    cdev->allocation_mode = 1;
    cdev->magic = LW_CDEV_MAGIC;
    return cdev;
}

int8_t lw_cdev_static_create(lw_cdev_t cdev,uint8_t type,void *config_data,void *lock)
{
    /* 不涉及线程安全，因此是线程安全的*/
    cdev_assert(cdev != NULL,-1);
    cdev_assert((lw_cdev_rtos && lw_cdev_auto_locking) ? (lock != NULL) : 1, NULL);
    lw_cdev_memset(cdev, 0x00, sizeof(struct lw_cdev));

    if(type >= LW_CDEV_TYPE_UNKNOWN) {
        cdev->cfg.error |= LW_CDEV_ERR_PARAM;
        cdev->type = LW_CDEV_TYPE_UNKNOWN;
    }
    else{
        cdev->type = type;
    }

#if lw_cdev_rtos && lw_cdev_auto_locking
    cdev->lock = lock;
#endif
    cdev->cfg.private_data = config_data;
    /* 标记为静态申请*/
    cdev->allocation_mode = 0;
    cdev->magic = LW_CDEV_MAGIC;
    return 0;
}

int8_t lw_cdev_register(lw_cdev_t cdev,const char *name, lw_cdev_adapter* adpt)
{
    /* 当前节点指针，初始化为线程链表的头指针*/
    cdev_assert(cdev != NULL && cdev->magic == LW_CDEV_MAGIC,-1);
    cdev_assert(name != NULL,-1);
#if lw_cdev_rtos
    cdev_assert(cdev_list_lock != NULL, -1);
#endif

    int8_t result = 0;
    lw_cdev_t curr = NULL;

    lw_cdev_spin_lock(cdev_list_lock);

    /* 判断名称是否复用*/
    curr = _lw_cdev_find(name);
    if (curr != NULL) {
        result = -3;
        goto __exit_unlock;
    }

    /* 判断当前设备是否已挂载到链表中*/
    curr = cdev_list;
    while (curr) {
        if (curr == cdev) {
            result = -4;
            goto __exit_unlock;
        }
        curr = curr->next;
    }

    /* 获取可用ID*/
    result = _lw_cdev_alloc_id();
    if (result < 0) {
        result = -5;
        goto __exit_unlock;
    }
    cdev->id = result;

    /* 单链表头结点头插法*/
    cdev->name = name;
    cdev->adpt = adpt;
    cdev->next = cdev_list;
    cdev_list = cdev;

__exit_unlock:
    lw_cdev_spin_unlock(cdev_list_lock);
    return result; 
}

int8_t lw_cdev_unregister(const char *name)
{
    cdev_assert(name != NULL,-1);
#if lw_cdev_rtos
    cdev_assert(cdev_list_lock != NULL, -1);
#endif

    lw_cdev_t curr = cdev_list;
    lw_cdev_t prev = NULL;
    int8_t result = 0;

    lw_cdev_spin_lock(cdev_list_lock);

    while (curr) {
        if (!_cdev_name_cmp(curr->name, name)) {
            /* 设备未关闭*/
            if(curr->ref_count){
                result = -2; 
                goto __exit_unlock;
            }
            else{
                _cdev_clear_id_used(curr->id);
                if (prev == NULL){
                    cdev_list = curr->next;     // 删除头节点
                }
                else{
                    prev->next = curr->next;    // 删除中间/尾节点
                }
            }
            result = 0;
            goto __exit_unlock;
        }
        prev = curr;
        curr = curr->next;
    }
    result = -1;

__exit_unlock:

    if (result == 0 && curr != NULL && curr->allocation_mode == 1) {
        lw_cdev_free(curr);
    }
    lw_cdev_spin_unlock(cdev_list_lock);
    return result; 
}

lw_cdev_t lw_cdev_find(const char *name)
{
    if (name == NULL) return NULL;

    lw_cdev_spin_lock(cdev_list_lock);
    lw_cdev_t dev = _lw_cdev_find(name);
    lw_cdev_spin_unlock(cdev_list_lock);

    return dev;
}

int8_t lw_cdev_init(lw_cdev_t cdev)
{
    int8_t result = 0;
    cdev_assert(cdev != NULL && cdev->magic == LW_CDEV_MAGIC,-1);
    cdev_assert(cdev->adpt != NULL,-1);
#if lw_cdev_rtos && lw_cdev_auto_locking
    cdev_assert(cdev->lock != NULL, -1);
#endif

    lw_cdev_mutex_lock(cdev->lock);
    if(cdev->adpt->init != NULL){
        /* 仅非活跃状态生效*/
        if(!(cdev->cfg.state & LW_CDEV_STATE_ACTIVATED)){
            result = cdev->adpt->init(&(cdev->cfg));
            if(result < 0){
                cdev->cfg.error |= LW_CDEV_ERR_FAULT;
            }
            else{
                cdev->cfg.state |= LW_CDEV_STATE_ACTIVATED;
            }
        }
    }
    lw_cdev_mutex_unlock(cdev->lock);
    return result;
}

int8_t lw_cdev_open(lw_cdev_t cdev,uint16_t open_flag)
{
    int8_t result = 0;
    cdev_assert(cdev != NULL && cdev->magic == LW_CDEV_MAGIC,-1);
    cdev_assert(cdev->adpt != NULL,-1);
#if lw_cdev_rtos && lw_cdev_auto_locking
    cdev_assert(cdev->lock != NULL, -1);
#endif

    open_flag &= LW_CDEV_FLAG_MASK;
    lw_cdev_mutex_lock(cdev->lock);

    /* 和lw_cdev_init不同，不论cdev->adpt->init是否存在均打开LW_CDEV_STATE_ACTIVATED*/
    if(!(cdev->cfg.state & LW_CDEV_STATE_ACTIVATED)){
        if(cdev->adpt->init != NULL){
            result = cdev->adpt->init(&(cdev->cfg));
            if(result < 0){
                cdev->cfg.error |= LW_CDEV_ERR_FAULT;
                goto __exit_unlock;
            }
        }
        cdev->cfg.state |= LW_CDEV_STATE_ACTIVATED;
    }
    /* 设备未打开或请求权限与原先不同*/
    if(!(cdev->cfg.state & LW_CDEV_STATE_OPENED)||
        (cdev->cfg.flag & LW_CDEV_FLAG_MASK) != (open_flag)){
        
        if(cdev->adpt->open != NULL){
            result = cdev->adpt->open(&(cdev->cfg),open_flag);
            if(result < 0){
                cdev->cfg.error |= LW_CDEV_ERR_FAULT;
            }
        }
        else{
            cdev->cfg.flag = open_flag;
        }
    }
    if(result >= 0){
        cdev->cfg.state |= LW_CDEV_STATE_OPENED;
        cdev->ref_count ++;
    }
    
__exit_unlock:

    lw_cdev_mutex_unlock(cdev->lock);

    return result;
}

int8_t lw_cdev_close(lw_cdev_t cdev)
{
    cdev_assert(cdev != NULL && cdev->magic == LW_CDEV_MAGIC,-1);
    cdev_assert(cdev->adpt != NULL,-1);
#if lw_cdev_rtos && lw_cdev_auto_locking
    cdev_assert(cdev->lock != NULL, -1);
#endif
    int8_t result = 0;

    lw_cdev_mutex_lock(cdev->lock);

    /* 当前已无设备打开*/
    if(cdev->ref_count == 0){
        result =  -1;
        goto __exit_unlock;
    }

    /* 引用降低*/
    cdev->ref_count --;
    if(cdev->adpt->close != NULL){
        result = cdev->adpt->close(&(cdev->cfg));
        if(result < 0){
            cdev->cfg.error |= LW_CDEV_ERR_FAULT;
        }
    }
    if(result == 0){
        /* 直接关闭*/
        cdev->cfg.state = LW_CDEV_STATE_CLOSED;
    }
__exit_unlock:
    lw_cdev_mutex_unlock(cdev->lock);
    return result;
}

ptrdiff_t lw_cdev_read(lw_cdev_t cdev, size_t pos,char *buf, size_t count, size_t timeout)
{
    cdev_assert(cdev != NULL && cdev->magic == LW_CDEV_MAGIC,-1);
    cdev_assert(cdev->adpt != NULL,-1);
    ptrdiff_t result = 0;

    lw_cdev_mutex_lock(cdev->lock);

    if(cdev->ref_count == 0) {
        result =  -1;
        goto __exit_unlock;
    }

    if(cdev->adpt->read != NULL){
        result = cdev->adpt->read(&(cdev->cfg),pos,buf,count,timeout);
        if(result < 0){
            cdev->cfg.error |= LW_CDEV_ERR_READ_FAIL;
        }
    }

__exit_unlock:
    lw_cdev_mutex_unlock(cdev->lock);
    return result;
}

ptrdiff_t lw_cdev_write(lw_cdev_t cdev, size_t pos, const char *buf, size_t count, size_t timeout)
{
    cdev_assert(cdev != NULL && cdev->magic == LW_CDEV_MAGIC,-1);
    cdev_assert(cdev->adpt != NULL,-1);
    ptrdiff_t result = 0;

    lw_cdev_mutex_lock(cdev->lock);

    if(cdev->ref_count == 0) {
        result =  -1;
        goto __exit_unlock;
    }

    if(cdev->adpt->write != NULL){
        result = cdev->adpt->write(&(cdev->cfg),pos,buf,count,timeout);
        if(result < 0){
            cdev->cfg.error |= LW_CDEV_ERR_WRITE_FAIL;
        }
    }

__exit_unlock:
    lw_cdev_mutex_unlock(cdev->lock);
    return result;
}

int8_t lw_cdev_ioctl(lw_cdev_t cdev, const uint8_t cmd, void *arg)
{
    cdev_assert(cdev != NULL && cdev->magic == LW_CDEV_MAGIC,-1);
    cdev_assert(cdev->adpt != NULL,-1);
    int8_t result = 0;

    lw_cdev_mutex_lock(cdev->lock);
    if(cdev->adpt->ioctl != NULL){
        result = cdev->adpt->ioctl(&(cdev->cfg),cmd,arg);
        if(result < 0){
            cdev->cfg.error |= LW_CDEV_ERR_UNSUPPORT;
        }
    }

    lw_cdev_mutex_unlock(cdev->lock);
    return result;
}

int8_t lw_cdev_get_id(lw_cdev_t cdev)
{
    cdev_assert(cdev != NULL && cdev->magic == LW_CDEV_MAGIC, -1);
    return cdev->id;
}

lw_cdev_type_e lw_cdev_get_type(lw_cdev_t cdev)
{
    cdev_assert(cdev != NULL && cdev->magic == LW_CDEV_MAGIC, LW_CDEV_TYPE_ERROR);
    return (lw_cdev_type_e)cdev->type;
}
