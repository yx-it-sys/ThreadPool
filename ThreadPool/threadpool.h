#pragma once

typedef struct threadPool ThreadPool;

// �����̳߳ز���ʼ��������ֵΪ��ַ���ͱ��⸴��
ThreadPool* threadPoolCreate(int min, int max, int queueSize);
// �����̳߳�

// ���̳߳��������

// ��ǰ�̳߳���æµ�̵߳ĸ���

// ��ȡ�̳߳��д����̵߳ĸ���

void* worker(void* arg);
void* manager(void* arg);