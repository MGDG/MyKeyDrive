/**
  ******************************************************************************
  * @file    MyKeyDrive.h
  * @author  mgdg
  * @version V1.0.0
  * @date    2017-09-04
  * @brief   
  ******************************************************************************
 **/

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "MyQueue.h"



#define MYKEY_CLICK             ((unsigned char)0x01U)		    //单击
#define MYKEY_DBLCLICK          ((unsigned char)0x02U)			//双击
#define MYKEY_LONG_PRESS        ((unsigned char)0x04U)			//长按
#define MYKEY_REPEAT            ((unsigned char)0x08U)			//连续触发、重复触发
#define MYKEY_RELASE			((unsigned char)0x10U)			//松开


/**
  * @brief	按键句柄
  * @remark	
  */
typedef void *MyKeyHandle;


/**
  * @brief	按键状态读取函数，按下返回true，弹起返回false
  * @remark	
  */
typedef bool (*KeyStatusFunc)(void); 


/**
  * @brief	初始化按键消息队列
  * @param	message_len: 消息队列的长度
  *
  * @return	bool 创建成功返回true，创建失败返回false
  * @remark	
  */
bool KeyMessage_Init(size_t message_len);


/**
  * @brief	注册一个按键
  * @param	*Key: 按键句柄
  * @param	func: 按键状态读取函数，按下返回true，弹起返回false
  * @param	Mode: 按键功能
  * @param	RepeatSpeed: 长按时连续触发周期，单位ms
  * @param	LongPressTime: 长按时间
  *
  * @return	bool: 按键注册状态，true表示注册成功，false表示注册失败
  * @remark	
  */
bool MyKey_KeyRegister(MyKeyHandle *Key,KeyStatusFunc func,unsigned char Mode,size_t RepeatSpeed,size_t LongPressTime);


/**
  * @brief	卸载一个按键
  * @param	*Key: 按键句柄
  *
  * @return	bool: 按键卸载状态，true表示卸载成功，false表示卸载失败
  * @remark
  */
bool MyKey_KeyUnregister(MyKeyHandle *Key);


/**
  * @brief	按键扫描,需要周期调用
  * @param	InterVal，调用间隔，单位ms
  *
  * @return	void
  * @remark	周期调用
  */
void MyKey_KeyScan(size_t InterVal);


/**
  * @brief	打印出已注册的按键ID
  * @param	void
  *
  * @return	void
  * @remark
  */
void MyKey_GetKeyInfo(void);


/**
  * @brief	从按键消息队列中获取一个按键消息
  * @param	*KeyID: 按键句柄
  * @param	*KeyEvent: 按键事件，单击、双击还是长按等等
  * @param	*KeyClickCount: 按键点击次数
  *
  * @return	bool 获取成功返回true，获取失败返回false
  * @remark	获取失败则表示按键队列中已经没有消息了
  */
bool KeyMessage_Get(MyKeyHandle *KeyID,unsigned char *KeyEvent,unsigned char *KeyClickCount);
