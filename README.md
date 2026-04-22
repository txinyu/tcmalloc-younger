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
## 性能测试
性能实测：我们的内存池 vs malloc

- 4个线程并发concurrent alloc&dealloc 4000000次
- 4个线程并发malloc&free 4000000次

### 测试代码
```
#include"ConcurrentAlloc.h"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <ctime>
#include <cstdio>
using namespace std;
 
// ntimes 一轮申请和释放内存的次数
// rounds 轮次
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;
 
	for (size_t k = 0; k < nworks; ++k)
	{
		// 【关键修复】全部引用捕获 [&]，绝对不能值捕获atomic
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
 
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc(16));
					//v.push_back(malloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();
 
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();
 
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}
 
	for (auto& t : vthread)
	{
		t.join();
	}
 
	// 【关键修复】.load() 取出原子里的无符号数值，适配%u
	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n",
		(unsigned int)nworks, (unsigned int)rounds, (unsigned int)ntimes, (unsigned int)malloc_costtime.load());
 
	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n",
		(unsigned int)nworks, (unsigned int)rounds, (unsigned int)ntimes, (unsigned int)free_costtime.load());
 
	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n",
		(unsigned int)nworks, (unsigned int)(nworks * rounds * ntimes), (unsigned int)(malloc_costtime.load() + free_costtime.load()));
}
 
 
// 单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;
 
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
 
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAlloc(16));
					//v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();
 
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();
 
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}
 
	for (auto& t : vthread)
	{
		t.join();
	}
 
	printf("%u个线程并发执行%u轮次，每轮次concurrent alloc %u次: 花费：%u ms\n",
		(unsigned int)nworks, (unsigned int)rounds, (unsigned int)ntimes, (unsigned int)malloc_costtime.load());
 
	printf("%u个线程并发执行%u轮次，每轮次concurrent dealloc %u次: 花费：%u ms\n",
		(unsigned int)nworks, (unsigned int)rounds, (unsigned int)ntimes, (unsigned int)free_costtime.load());
 
	printf("%u个线程并发concurrent alloc&dealloc %u次，总计花费：%u ms\n",
		(unsigned int)nworks, (unsigned int)(nworks * rounds * ntimes), (unsigned int)(malloc_costtime.load() + free_costtime.load()));
}
 
int main()
{
	size_t n = 100000;
	cout << "==========================================================" << endl;
	BenchmarkConcurrentMalloc(n, 4, 10);
	cout << endl << endl;
 
	BenchmarkMalloc(n, 4, 10);
	cout << "==========================================================" << endl;
 
	return 0;
}
```
### 测试结果
<img width="1463" height="453" alt="image" src="https://github.com/user-attachments/assets/845d2460-707b-4652-a96b-ca760dfb0517" />

- malloc 总耗时：10742 ms
- 我们的并发内存池：5555 ms
- 提速 2 倍，线程越多差距越大
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
## 相关文档

项目详细优化原理、源码解析与实现细节，可参考我的技术博客：

[高并发内存池 - 简化版 tcmalloc](https://blog.csdn.net/m0_62807361/article/details/160281022?spm=1001.2014.3001.5502)
