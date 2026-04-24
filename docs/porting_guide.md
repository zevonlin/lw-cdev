# LW-CDEV移植说明


---



| 日期 | 作者 | 说明 |
| :---: | :---: | --- |
| 2026.04.24 | zevonlin | 移植说明初版 |
| | | |


## 移植方式:
所需移植的文件内容位于`/src`文件夹内

```c
── src/                                # 框架核心源码目录
   ├── lw_cdev.h                       # 框架对外统一头文件（应用层仅需包含此文件）
   ├── lw_cdev_def.h                   # 核心类型定义
   └── lw_cdev.c                       # 核心框架实现
```



其中,移植文件位于`lw_cdev_def.h`,需定义如下内容:



### 文件裁剪
对于文件来说有如下内容需要裁剪,以下宏仅有0与1可选项,0标识关闭,1标识开启

```c
#define lw_cdev_rtos            1
#define lw_cdev_auto_locking    1
#define lw_cdev_user            0
#define lw_cdev_assertion       1
```

+ `lw_cdev_rtos`:当设备框架运行在多线程系统内部时,需要打开该宏以避免设备框架的相关函数出现线程并行冲突.
+ `lw_cdev_auto_locking`:当设备框架运行在多线程系统内部时,该选项将有效.
+ 若开启该选项,设备框架将使用互斥锁,自动维护设备控制类函数的线程安全,且不允许在其回调函数内嵌套调用.否则将会出现死锁.
+ 若关闭该选项,设备框架将不再自动维护设备控制类函数的线程安全,用户需自行维护控制类函数的线程安全以避免异常.
+  非大量设备并行访问的场景下建议开启；若需极致优化函数执行速度，可关闭，但需自行承担线程安全维护责任.建议开启.
+ `lw_cdev_user`:开启后,将允许设备额外携带一个用户数据.
+ `lw_cdev_assertion`:开启后,设备框架将会检查参数值是否合规,建议在非正式版本时开启,发布前关闭.



### 接口适配
对于文件框架来说,有如下函数需要适配

```c
#define lw_cdev_malloc(size)       malloc(size)
#define lw_cdev_free(mem)          free(mem)
#define lw_cdev_memset(s,c,count)  memset(s,c,count)
#define lw_cdev_strncmp(s1,s2,n)   strncmp(s1,s2,n)
#define lw_cdev_strlen(s)          strlen(s)
```

当用户拥有自己的实现函数时,例如在多线程中拥有`my_malloc`,则需进行如下修改,避免线程安全异常

```c
#define lw_cdev_malloc(size)       my_malloc(size)
```



在rtos中,有如下函数需要适配

```c
#if lw_cdev_rtos
#define lw_cdev_spin_lock(lock)    
#define lw_cdev_spin_unlock(lock)  
#else
#define lw_cdev_spin_lock(lock)    ((void)0)
#define lw_cdev_spin_unlock(lock)  ((void)0)
#endif

#if lw_cdev_rtos && lw_cdev_auto_locking
#define lw_cdev_mutex_lock(lock)   
#define lw_cdev_mutex_unlock(lock) 
#else
#define lw_cdev_mutex_lock(lock)   ((void)0)
#define lw_cdev_mutex_unlock(lock) ((void)0)
#endif
```

`lw_cdev_spin_lock`:获取一把自旋锁,实际使用时可根据需求自行更换为任何获取锁的方式,仅需注意锁的初始化需自行实现.

`lw_cdev_spin_unlock`:释放一把自旋锁,实际使用时可根据需求自行更换为任何获取锁的方式,仅需注意锁的初始化需自行实现.

`lw_cdev_mutex_lock`:获取一把互斥锁,实际使用时可根据需求自行更换为任何获取锁的方式,仅需注意锁的初始化需自行实现.

`lw_cdev_mutex_unlock`:释放一把互斥锁,实际使用时可根据需求自行更换为任何获取锁的方式,仅需注意锁的初始化需自行实现.



当断言开启时,有如下函数可自行修改或适配,以实现打印/等待等操作.

```c
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
```



## 调用方式
当接口文件适配完成后即可完成移植,仅需调用`lw_cdev.h`头文件便可开启框架使用.

例如

```c
#include <windows.h>
#include "lw_cdev.h"

static CRITICAL_SECTION demo_core_lock;

int main() {
    InitializeCriticalSectionAndSpinCount(&demo_core_lock, 4000);

	printf("hello,lw_rtos\r\n");
    lw_cdev_manager_init(&demo_core_lock);


	while (1) {
		Sleep(1000);
	}
	return 0;
}
```



