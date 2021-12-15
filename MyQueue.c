/**
  * @file       MyQueue.c
  * @author     mgdg
  * @brief      队列驱动文件
  * @version    v1.1
  * @date       2019-09-12
  * @remark     创建可以存放任意元素的环形队列，包含了入队出队等基本操作
  */

/*只有一个生产者和消费者的情况下不需要锁保护*/
//#define MYQUEUE_USE_LOCK

#include "MyQueue.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef MYQUEUE_USE_LOCK
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MYQUEUE_API_CREATELOCK(v)       do{(v)->lock = xSemaphoreCreateMutex();assert((v)->lock);}while(0);
#define MYQUEUE_API_DELETELOCK(v)       do{vSemaphoreDelete((v)->lock);}while(0);
#define MYQUEUE_API_LOCK(v)             do{xSemaphoreTake((v)->lock, portMAX_DELAY);}while(0);
#define MYQUEUE_API_UNLOCK(v)           do{xSemaphoreGive((v)->lock);}while(0);
#else
#define MYQUEUE_API_CREATELOCK(v)
#define MYQUEUE_API_DELETELOCK(v)
#define MYQUEUE_API_LOCK(v)
#define MYQUEUE_API_UNLOCK(v)
#endif

#define debug_i(format,...)             /*printf(format"\n",##__VA_ARGS__)*/
#define NUM_IN_QUEUE(v)                 ((((v)->front) <= ((v)->rear))?(((v)->rear)-((v)->front)):(((v)->len)-((v)->front)+((v)->rear)))
#define LFET_NUM_IN_QUEUE(v)            ((((v)->front) <= ((v)->rear))?((((v)->len)-1)-(((v)->rear)-((v)->front))):(((v)->front)-((v)->rear)-1))

struct myQueue {
    void                *buffer;        /*数据缓冲区*/
    size_t              len;            /*队列长度*/
    size_t              size;           /*单个数据大小(单位 字节)*/
    size_t              front;          /*数据头,指向下一个空闲存放地址*/
    size_t              rear;           /*数据尾，指向第一个数据*/
#ifdef MYQUEUE_USE_LOCK
    SemaphoreHandle_t   lock;           /*保护锁*/
#endif
};

myQueueHandle_t myQueueCreate(size_t queue_len, size_t item_size)
{
    if ((0 == queue_len) || (0 == item_size)) {
        debug_i("create queue,len=%d,ItemSize=%d", queue_len, item_size);
        return NULL;
    }
    myQueueHandle_t queue = calloc(1, sizeof(struct myQueue));
    assert(queue);
    queue->size = item_size;
    queue->len = queue_len;
    queue->buffer = malloc((queue->size) * (queue->len));
    assert(queue->buffer);
    queue->front = queue->rear = 0;
    MYQUEUE_API_CREATELOCK(queue);
    MYQUEUE_API_UNLOCK(queue);
    return queue;
}

void myQueueDelete(myQueueHandle_t queue)
{
    if (NULL == queue) {
        return;
    }
    MYQUEUE_API_LOCK(queue);
    free(queue->buffer);
    MYQUEUE_API_UNLOCK(queue);
    MYQUEUE_API_DELETELOCK(queue);
    free(queue);
}

size_t  myQueueNum(const myQueueHandle_t queue)
{
    if (NULL == queue) {
        return 0;
    }
    MYQUEUE_API_LOCK(queue);
    size_t num_in_queue = NUM_IN_QUEUE(queue);
    MYQUEUE_API_UNLOCK(queue);
    return num_in_queue;
}

size_t myQueueLeftNum(const myQueueHandle_t queue)
{
    if (NULL == queue) {
        return 0;
    }
    MYQUEUE_API_LOCK(queue);
    size_t left_num_in_queue = LFET_NUM_IN_QUEUE(queue);
    MYQUEUE_API_UNLOCK(queue);
    return left_num_in_queue;
}

size_t myQueueCapacity(const myQueueHandle_t queue)
{
    return (NULL == queue) ? 0 : (queue->len - 1);
}

bool myQueueIsFull(const myQueueHandle_t queue)
{
    if (NULL == queue) {
        return false;
    }
    return (myQueueNum(queue)  == ((queue->len) - 1));
}

bool myQueueIsEmpty(const myQueueHandle_t queue)
{
    return (NULL == queue) ? false : (0 == myQueueNum(queue));
}

static  __attribute__((always_inline)) inline size_t minn(size_t a, size_t b)
{
    return a < b ? a : b;
}

static void *myQueue_memcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}

bool myQueuePut(myQueueHandle_t queue, const void *buf, size_t num)
{
    if ((NULL == queue) || (NULL == queue->buffer) || (NULL == buf) || (0 == num)) {
        debug_i("put queue=%p,buf=%p,num=%d", queue, buf, num);
        return false;
    }
    bool rt = false;
    MYQUEUE_API_LOCK(queue);
    size_t left_num_in_queue = LFET_NUM_IN_QUEUE(queue);
    if (num <= left_num_in_queue) {
        size_t templen = minn((queue->len) - (queue->rear), num);
        if (templen > 0) {
            myQueue_memcpy((char *)(queue->buffer) + (queue->rear) * (queue->size), buf, templen * (queue->size));
        }
        if (num > templen) {
            myQueue_memcpy((char *)(queue->buffer), (char *)buf + templen * (queue->size), (num - templen) * (queue->size));
        }
        queue->rear = (queue->rear + num) % (queue->len);
        // p->rear += num;
        // if (p->rear >= p->len) {
        //  p->rear -= p->len;
        // }
        rt = true;
    } else {
        debug_i("put failed, num=%d,left=%d,front=%d,rear=%d,len=%d", num, left_num_in_queue, queue->front, queue->rear, queue->len);
    }
    MYQUEUE_API_UNLOCK(queue);
    return rt;
}

bool myQueueGet(myQueueHandle_t queue, void *buf, size_t num)
{
    if ((NULL == queue) || (NULL == queue->buffer) || (NULL == buf) || (0 == num)) {
        debug_i("get queue=%p,buf=%p,num=%d", queue, buf, num);
        return false;
    }
    bool rt = false;
    MYQUEUE_API_LOCK(queue);
    size_t num_in_queue = NUM_IN_QUEUE(queue);
    if (num <= num_in_queue) {
        size_t templen = minn((queue->len) - (queue->front), num);
        if (templen > 0) {
            myQueue_memcpy((char *)buf, (char *)(queue->buffer) + (queue->front) * (queue->size), templen * (queue->size));
        }
        if (num > templen) {
            myQueue_memcpy((char *)buf + templen * (queue->size), queue->buffer, (num - templen) * (queue->size));
        }
        queue->front = (queue->front + num) % (queue->len);
        // p->front += num;
        // if (p->front >= p->len) {
        //  p->front -= p->len;
        // }
        rt = true;
    } else {
        debug_i("get failed, num=%d,inQueue=%d,front=%d,rear=%d,len=%d", num, num_in_queue, queue->front, queue->rear, queue->len);
    }
    MYQUEUE_API_UNLOCK(queue);
    return rt;
}

bool myQueuePeek(const myQueueHandle_t queue, void *buf, size_t num, size_t offset)
{
    if ((NULL == queue) || (NULL == queue->buffer) || (NULL == buf) || (0 == num)) {
        debug_i("peek queue=%p,buf=%p,num=%d", queue, buf, num);
        return false;
    }
    bool rt = false;
    MYQUEUE_API_LOCK(queue);
    size_t num_in_queue = NUM_IN_QUEUE(queue);
    if ((offset + num) <= num_in_queue) {
        size_t temp_front = (queue->front) + offset;
        size_t templen = minn((queue->len) - temp_front, num);
        if (templen > 0) {
            myQueue_memcpy((char *)buf, (char *)(queue->buffer) + temp_front * (queue->size), templen * (queue->size));
        }
        if (num > templen) {
            myQueue_memcpy((char *)buf + templen * (queue->size), queue->buffer, (num - templen) * (queue->size));
        }
        rt = true;
    } else {
        debug_i("get failed, num=%d,inQueue=%d,front=%d,rear=%d,len=%d", num, num_in_queue, queue->front, queue->rear, queue->len);
    }
    MYQUEUE_API_UNLOCK(queue);
    return rt;
}

bool myQueuePop(myQueueHandle_t queue, size_t num)
{
    if ((NULL == queue) || (0 == num)) {
        debug_i("pop queue=%p,num=%d", queue, num);
        return false;
    }
    bool rt = true;
    MYQUEUE_API_LOCK(queue);
    size_t num_in_queue = NUM_IN_QUEUE(queue);
    if (num <= num_in_queue) {
        queue->front = (queue->front + num) % (queue->len);
    } else {
        rt = false;
        debug_i("pop failed, num=%d,inQueue=%d,front=%d,rear=%d,len=%d", num, num_in_queue, queue->front, queue->rear, queue->len);
    }
    MYQUEUE_API_UNLOCK(queue);
    return rt;
}

bool myQueuePopAll(myQueueHandle_t queue)
{
    if (NULL == queue) {
        return false;
    }
    MYQUEUE_API_LOCK(queue);
    queue->front = queue->rear;
    MYQUEUE_API_UNLOCK(queue);
    return true;
}

