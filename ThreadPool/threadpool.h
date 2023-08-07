#pragma once

typedef struct threadPool ThreadPool;

// 创建线程池并初始化，返回值为地址类型避免复制
ThreadPool* threadPoolCreate(int min, int max, int queueSize);
// 销毁线程池

// 给线程池添加任务

// 当前线程池中忙碌线程的个数

// 获取线程池中存活的线程的个数

void* worker(void* arg);
void* manager(void* arg);