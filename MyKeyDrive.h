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
#include <stdbool.h>

#define KEY_FILTER_TIME   30			//消抖滤波时间，单位ms
#define KEY_DBL_INTERVAL	250			//双击最大间隔时间，单位ms

//定义按键触发模式
#define MYKEY_CLICK             ((uint8_t)0x01U)		    //单击
#define MYKEY_DBLCLICK          ((uint8_t)0x02U)			//双击
#define MYKEY_LONG_PRESS        ((uint8_t)0x04U)			//长按
#define MYKEY_REPEAT            ((uint8_t)0x08U)			//连续触发、重复触发


typedef bool (*KeyStatusFunc)(void); 


uint32_t MyKey_KeyRegister(KeyStatusFunc func,uint8_t Mode,uint32_t RepeatSpeed,uint32_t LongPressTime);
void MyKey_KeyScan(uint32_t InterVal);
bool MyKey_KeyUnregister(int32_t KeyID);
void MyKey_GetKeyInfo(void);
void KeyMessage_Init(uint8_t message_len);
bool KeyMessage_Get(uint32_t *KeyID,uint8_t *KeyEvent,uint8_t *KeyClickCount);
bool KeyMessage_Peek(uint32_t *KeyID,uint8_t *KeyEvent,uint8_t *KeyClickCount);

