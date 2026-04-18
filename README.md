# 高并发内存池 - 简化版 tcmalloc

基于 Google tcmalloc 核心思想实现的高并发内存池，用于学习与替代系统 malloc/free，提升多线程场景下的内存分配效率。

---

## 项目介绍

本项目是 **tcmalloc（Thread-Caching Malloc）** 的精简实现，剥离复杂逻辑，保留核心框架：
- ThreadCache：线程本地缓存，无锁快速分配
- CentralCache：中心缓存，全局共享，桶锁机制
- PageCache：页缓存，以页为单位管理内存，合并页减少碎片

相比系统原生 malloc：
- 高并发场景下锁竞争更少
- 内存分配速度更快
- 有效降低内存碎片

---

## 核心结构

### 1. ThreadCache（线程缓存）
- 每个线程独有一份，无锁访问
- 管理 <= 256KB 的小内存块
- 哈希桶 + 自由链表管理不同大小内存

### 2. CentralCache（中心缓存）
- 全局所有线程共享
- 按需给 ThreadCache 批量供给内存
- 桶锁降低并发冲突

### 3. PageCache（页缓存）
- 以页（8KB）为单位管理内存
- 向系统申请大块内存
- 内存释放后合并相邻页，缓解外碎片

---

## 技术要点
- C++ 单例模式
- 线程本地存储 TLS
- 互斥锁 / 桶锁
- 自由链表 + 哈希桶
- 内存对齐与大小映射
- 页管理与 Span 结构
- 基数树优化页映射（可选扩展）

---

## 项目文件

```
CentralCache.cpp # 中心缓存实现
CentralCache.h
Common.h # 通用定义
ConcurrentAlloc.h # 对外分配接口
ObjectPool.h # 定长内存池
PageCache.cpp # 页缓存实现
PageCache.h
PageMap.cpp # 页映射
ThreadCache.cpp # 线程缓存实现
ThreadCache.h
text.cpp # 测试与性能对比
tcmalloc.sln/.vcxproj VS 项目文件
```


---

## 性能说明
- 多线程高并发场景下，内存分配速度显著优于系统 malloc
- 小内存分配无锁
- 大内存通过页缓存高效管理
- 有效降低锁竞争与内存碎片

---

## 使用方式
```cpp
#include "ConcurrentAlloc.h"

// 分配内存
void* ptr = ConcurrentAlloc(size);

// 释放内存
ConcurrentFree(ptr);
```

## 适用场景
- 高并发服务器
- 频繁小内存分配
- 游戏 / 中间件 / 基础库
- 学习内存池、tcmalloc 原理
## 扩展方向
- 替换系统 malloc（weak alias /hook）
- 多平台支持 Linux / Windows
- 加入更完善的异常与检测
- 实现完整基数树页映射
- 性能监控与统计
## 备注
本项目为学习向实现，核心逻辑与结构对齐 tcmalloc，适合深入理解高并发内存管理。
