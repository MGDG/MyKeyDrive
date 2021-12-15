/**
  * @file       MyQueue.h
  * @author     mgdg
  * @brief      队列驱动文件
  * @version    v1.1
  * @date       2019-09-12
  * @remark     创建可以存放任意元素的环形队列，包含了入队出队等基本操作
  */

#ifndef __MYQUEUE_H
#define __MYQUEUE_H

#include <stdlib.h>
#include <stdbool.h>

/*队列句柄*/
typedef struct myQueue *myQueueHandle_t;

/**
  * @brief  创建队列
  * @param  queue_len：队列长度，存储的元素个数
  * @param  item_size：队列单个元素的大小，以字节为单位

  * @return myQueueHandle_t：队列句柄
  * @remark
  */
myQueueHandle_t myQueueCreate(size_t queue_len, size_t item_size);

/**
  * @brief  删除队列
  * @param  queue：队列句柄

  * @return void
  * @remark
  */
void myQueueDelete(myQueueHandle_t queue);

/**
  * @brief  获取队列已存放数据个数
  * @param  queue：队列句柄
  *
  * @return size_t:队列数据个数
  * @remark
  */
size_t myQueueNum(const myQueueHandle_t queue);

/**
  * @brief  获取队列剩余可存放数据个数
  * @param  queue：队列句柄
  *
  * @return size_t:队列剩余可存放数据个数
  * @remark
  */
size_t myQueueLeftNum(const myQueueHandle_t queue);

/**
  * @brief  获取队列总容量
  * @param  queue：队列句柄
  *
  * @return size_t:队列容量
  * @remark
  */
size_t myQueueCapacity(const myQueueHandle_t queue);

/**
  * @brief  队列是否已满
  * @param  queue：队列句柄
  *
  * @return bool:队列满状态
  * @remark
  */
bool myQueueIsFull(const myQueueHandle_t queue);

/**
  * @brief  队列是否已空
  * @param  *queue：队列句柄
  *
  * @return bool:队列空状态
  * @remark
  */
bool myQueueIsEmpty(const myQueueHandle_t queue);

/**
  * @brief  将指定个数的数据放入队列
  * @param  queue：队列句柄
  * @param  *buf：数据指针
  * @param  num：数据个数
  *
  * @return bool：是否放入成功
  * @remark
  */
bool myQueuePut(myQueueHandle_t queue, const void *buf, size_t num);

/**
  * @brief  从队列中取出指定个数的数据
  * @param  queue：队列句柄
  * @param  *buf：数据指针
  * @param  num：取出的队列个数
  *
  * @return bool：是否取出成功
  * @remark
  */
bool myQueueGet(myQueueHandle_t queue, void *buf, size_t num);

/**
  * @brief  从指定偏移位置开始，从队列中获取指定个数的数据，但是不弹出数据
  * @param  queue：队列句柄
  * @param  *buf
  * @param  num：数据长度
  * @param  offset：偏移位置
  *
  * @return bool：是否获取成功
  * @remark
  */
bool myQueuePeek(const myQueueHandle_t queue, void *buf, size_t num, size_t offset);

/**
  * @brief  弹掉指定个数的数据
  * @param  queue：队列句柄
  * @param  num：长度
  *
  * @return bool：是否弹出成功
  * @remark
  */
bool myQueuePop(myQueueHandle_t queue, size_t num);

/**
  * @brief  POP所有数据
  * @param  queue：队列句柄
  *
  * @return bool：是否弹出成功
  * @remark
  */
bool myQueuePopAll(myQueueHandle_t queue);

#endif
