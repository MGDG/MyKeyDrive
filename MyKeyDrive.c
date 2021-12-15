/**
 * @file MyKeyDrive.c
 * @author MGDG
 * @brief 注册一个按键，注册时选择按键支持的检测方式，如单击、双击、长按、连续触发、长按时间、连续触发间隔。
 *        按键消息用到了队列驱动
 * @version 0.1
 * @date 2017-09-04
 *
 * @copyright
 *
 */

#include "stdio.h"
#include "MyKeyDrive.h"
#include "MyQueue.h"

#define KEY_EVENT_MSG_QUEUE_SIZE        (10)    /** 按键事件消息队列长度 */
#define KEY_FILTER_TIME                 (30)    /** 消抖滤波时间，单位ms */
#define KEY_DBL_INTERVAL                (250)   /** 双击最大间隔时间，单位ms */

//按键状态
typedef enum {
    KEYSTATE_RELASE = 0,                        /** 按键松开 */
    KEYSTATE_PRESS_SD,                          /** 按键按下，支持单击和双击模式 */
    KEYSTATE_PRESS_D,                           /** 按键按下，支持双击模式 */
    KEYSTATE_PRESS_S,                           /** 按键按下，支持单击模式 */
    KEYSTATE_PRESS_LR,                          /** 按键按下，支持长按和连续触发模式 */
    KEYSTATE_PRESS_L,                           /** 按键按下，支持长按模式 */
    KEYSTATE_PRESS_R,                           /** 按键按下，支持连续触发模式 */
} myKeyState_t;

//按键消息结构体定义
typedef struct {
    MyKeyHandle KeyID;                          /** 按键ID */
    unsigned char KeyEvent;                     /** 按键事件 */
    unsigned char KeyClickCount;                /** 按键次数计数 */
} myKeyMsg_t;

//按键属性
typedef struct myKey {
    MyKeyHandle KeyID;                          /** 按键对应的ID，每个按键对应唯一的ID */
    KeyStatusFunc KeyStatus;                    /** 按键按下的判断函数,1表示按下,初始化时指定 */
    struct myKey *Next_Key;                     /** 下一个按键 */
    size_t PressTime;                           /** 按键按下持续时间（ms） */
    size_t FilterCount;                         /** 消抖滤波计时（ms） */
    size_t RepeatSpeed;                         /** 连续触发周期（ms），初始化时指定 */
    size_t RepeatCount;                         /** 连续触发周期计时（ms） */
    size_t LongPressTime;                       /** 长按时间，超过该时间认为是长按（ms），初始化时指定 */
    size_t DblClkCount;                         /** 双击间隔时间计时（ms） */

    int keyState;                               /** 消抖后的按键状态,1表示按下,0表示弹起 */
    unsigned char ClickCount;                   /** 连按次数计数 */
    unsigned char Mode;                         /** 按键支持的检测模式，初始化时指定 */
    myKeyState_t State;                         /** 按键当前状态 */
} myKey_t;

static myKey_t *MyKeyList = NULL;               /** 已注册的按键链表 */
static myQueueHandle_t KeyBufQueue = NULL;      /** 按键事件队列 */
static volatile int MyKeyLock = 0;              /** TODO: 保护锁,无操作系统环境下需要实现 */

static bool KeyList_Put(myKey_t **ListHead, MyKeyHandle KeyID)
{
    myKey_t *p = NULL;
    myKey_t *q = NULL;

    if (KeyID == NULL) {
        return false;
    }

    if (*ListHead == NULL) {        //链表为空
        p = (myKey_t *)KeyID;
        p->KeyID = KeyID;
        p->Next_Key = NULL;
        *ListHead = p;
        return true;
    }

    q = *ListHead;
    p = (*ListHead)->Next_Key;
    while (p != NULL) {
        if (q->KeyID == KeyID) {            //链表中已经存在
            return true;
        }
        q = p;
        p = p->Next_Key;
    }

    //链表中不存在，链接进去
    p = (myKey_t *)KeyID;
    //初始化节点数据
    p->KeyID = KeyID;
    p->Next_Key = NULL;
    //加入链表
    q->Next_Key = p;
    return true;
}


int MyKey_Init(void)
{
    KeyBufQueue = myQueueCreate(KEY_EVENT_MSG_QUEUE_SIZE, sizeof(myKeyMsg_t));
    if (KeyBufQueue) {
        printf("Key message queue create success, queue len %d\r\n", KEY_EVENT_MSG_QUEUE_SIZE);
        return 0;
    }
    printf("Key message queue create failed\r\n");
    return -1;
}

int MyKey_Deinit(void)
{
    myQueueDelete(KeyBufQueue);
    myKey_t *p;
    while (MyKeyList) {
        p = MyKeyList;
        MyKeyList = MyKeyList->Next_Key;
        free(p);
    }
}

static bool KeyMessage_Put(MyKeyHandle KeyID, unsigned char KeyEvent, unsigned char ClickCount)
{
    myKeyMsg_t temp;
    temp.KeyID = KeyID;
    temp.KeyEvent = KeyEvent;
    temp.KeyClickCount = ClickCount;
    return myQueuePut(KeyBufQueue, &temp, 1);
}

int MyKey_Read(MyKeyHandle *KeyID, unsigned char *KeyEvent, unsigned char *KeyClickCount)
{
    myKeyMsg_t temp;
    if ( myQueueGet(KeyBufQueue, &temp, 1)) {
        *KeyID = temp.KeyID;
        *KeyEvent = temp.KeyEvent;
        *KeyClickCount = temp.KeyClickCount;
        return 0;
    }
    return -1;
}

int MyKey_Register(MyKeyHandle *Key, KeyStatusFunc func, unsigned char Mode, size_t RepeatSpeed, size_t LongPressTime)
{
    //先检查按键是否已经被注册过了
    myKey_t *p = MyKeyList;
    while (p != NULL) {
        if (p->KeyStatus == func) {
            return -1;
        }
        p = p->Next_Key;
    }

    myKey_t *NewKey = (myKey_t *)malloc(sizeof(myKey_t));
    if (NewKey == NULL) {
        return -1;
    }
    NewKey->KeyStatus = func;
    NewKey->KeyID = (MyKeyHandle)NewKey;
    NewKey->Mode = Mode;
    NewKey->State = KEYSTATE_RELASE;
    NewKey->PressTime = 0;
    NewKey->FilterCount = 0;
    NewKey->RepeatSpeed = RepeatSpeed;
    NewKey->RepeatCount = 0;
    NewKey->LongPressTime = LongPressTime;
    NewKey->ClickCount = 0;
    NewKey->keyState = 0;
    NewKey->DblClkCount = 0;
    NewKey->Next_Key = NULL;

    if (KeyList_Put(&MyKeyList, NewKey)) {
        *Key = (MyKeyHandle)NewKey;
        return 0;
    } else {
        free(NewKey);
        return -1;
    }
}

int MyKey_Unregister(MyKeyHandle *Key)
{
    myKey_t *TempNode = (myKey_t *)(*Key);
    myKey_t *p = NULL;
    myKey_t *q = MyKeyList;

    if (TempNode == NULL) {         //无效指定节点
        return -1;
    }

    //查找是否存在
    while (q != NULL) {
        if (q->KeyID == *Key) {
            break;
        }
        q = q->Next_Key;
    }
    if (q == NULL) {
        return -1;
    }
    q = MyKeyList;

    //存在
    if (MyKeyList == NULL) {        //无效链表头节点
        if (TempNode->KeyID == *Key) {
            free(TempNode);             //释放掉被删除的节点
            *Key = NULL;
            return 0;
        }
        return -1;
    }

    while (q != TempNode && q != NULL) {
        p = q;
        q = q->Next_Key;
    }

    if (q != TempNode) {            //链表中不存在该节点
        if (TempNode->KeyID == *Key) {
            free(TempNode);             //释放掉被删除的节点
            *Key = NULL;
            return 0;
        }
        return -1;
    }

    if (q->KeyID == *Key) {
        if (q == MyKeyList) {           //删除的是第一个节点
            MyKeyList = MyKeyList->Next_Key;
        } else {
            p->Next_Key = q->Next_Key;
        }
        free(q);                        //释放掉被删除的节点
        *Key = NULL;
        return 0;
    }
    return -1;
}

void MyKey_PrintKeyInfo(void)
{
    myKey_t *q = MyKeyList;
    if (q == NULL) {
        printf("NO KEY\r\n");
    }
    while (q != NULL) {
        printf("KEY ID : %p\r\n", q->KeyID);
        q = q->Next_Key;
    }
}

void MyKey_Scan(size_t InterVal)
{
    myKey_t *p = MyKeyList;
    while (p != NULL) {
        if (p->KeyStatus() == 1) {
            //按下消抖
            if (p->FilterCount < KEY_FILTER_TIME) {
                p->FilterCount = KEY_FILTER_TIME;
            } else if (p->FilterCount < (KEY_FILTER_TIME + KEY_FILTER_TIME)) {
                p->FilterCount += InterVal;
            } else {
                //消抖时间已到，上一次状态为弹起
                if (p->keyState == 0) {
                    p->keyState = 1;
                    //第一次按下
                    if (p->State == KEYSTATE_RELASE) {
                        p->ClickCount = 1;
                        p->DblClkCount = 0;         //双击等待时间清除
                        p->PressTime = 0;           //长按计时复位
                        p->RepeatCount = 0;         //连续触发计时复位
                        //支持单击和双击
                        if ( ((p->Mode)&MYKEY_EVENT_CLICK) && ((p->Mode)&MYKEY_EVENT_DBLCLICK) ) {
                            p->State = KEYSTATE_PRESS_SD;
                        } else if ((p->Mode)&MYKEY_EVENT_CLICK) {
                            //仅支持单击
                            p->State = KEYSTATE_PRESS_S;
                            //即不支持长按，也不支持连续触发
                            if (!( ((p->Mode)&MYKEY_EVENT_LONG_PRESS) || ((p->Mode)&MYKEY_EVENT_REPEAT) )) {
                                //发送单击按键消息
                                KeyMessage_Put(p->KeyID, MYKEY_EVENT_CLICK, p->ClickCount);
                            }
                        } else if ((p->Mode)&MYKEY_EVENT_DBLCLICK) {
                            //仅支持双击
                            p->State = KEYSTATE_PRESS_D;
                        }
                    } else if (p->State == KEYSTATE_PRESS_SD  || p->State == KEYSTATE_PRESS_D) {
                        //上一次为支持双击按下（支持单击和双击、仅支持双击）
                        p->DblClkCount = 0;                     //连击间隔时间清0
                        //连续按次数加1
                        if (p->ClickCount < 255) {
                            p->ClickCount++;
                        }
                    }
                } else {
                    //持续按住
                    //同时使能长按和连续触发，长按时间到达之后开始连续触发，不发送长按消息
                    if ( ((p->Mode)&MYKEY_EVENT_LONG_PRESS) && ((p->Mode)&MYKEY_EVENT_REPEAT) ) {
                        if (p->PressTime < p->LongPressTime) {
                            p->PressTime += InterVal;
                            if (p->PressTime >= p->LongPressTime) {
                                p->RepeatCount = 0;             //重复触发计时清0
                                p->State = KEYSTATE_PRESS_LR;
                                //发送按键长按消息
                                //KeyMessage_Put(p->KeyID,MYKEY_EVENT_LONG_PRESS);
                            }
                        } else {
                            p->RepeatCount += InterVal;
                            if (p->RepeatCount >= p->RepeatSpeed) {
                                p->RepeatCount = 0;
                                p->State = KEYSTATE_PRESS_LR;
                                //发送连续按键消息
                                KeyMessage_Put(p->KeyID, MYKEY_EVENT_REPEAT, p->ClickCount);
                                if (p->ClickCount < 255) {
                                    p->ClickCount++;
                                }
                            }
                        }
                    } else if ((p->Mode)&MYKEY_EVENT_LONG_PRESS) {
                        //只使能长按功能
                        if (p->PressTime < p->LongPressTime) {
                            p->PressTime += InterVal;
                            if (p->PressTime >= p->LongPressTime) {
                                p->State = KEYSTATE_PRESS_L;
                                //发送按键长按消息
                                KeyMessage_Put(p->KeyID, MYKEY_EVENT_LONG_PRESS, p->ClickCount);
                            }
                        }
                    } else if ((p->Mode)&MYKEY_EVENT_REPEAT) {
                        //只使能连发功能
                        p->RepeatCount += InterVal;
                        if (p->RepeatCount >= p->RepeatSpeed) {
                            p->RepeatCount = 0;
                            p->State = KEYSTATE_PRESS_R;
                            //发送连续按键消息
                            KeyMessage_Put(p->KeyID, MYKEY_EVENT_REPEAT, p->ClickCount);
                            if (p->ClickCount < 255) {
                                p->ClickCount++;
                            }
                        }
                    }
                }
            }
        } else {
            //弹起消抖
            if (p->FilterCount > KEY_FILTER_TIME) {
                p->FilterCount = KEY_FILTER_TIME;
            } else if (p->FilterCount != 0) {
                if (p->FilterCount >= InterVal) {
                    p->FilterCount -= InterVal;
                } else {
                    p->FilterCount = 0;
                }
            } else {
                //消抖时间到
                p->keyState = 0;
                switch (p->State) {
                    //支持单击和双击
                    case KEYSTATE_PRESS_SD: {
                        p->DblClkCount += InterVal;
                        //超过时间没有双击
                        if (p->DblClkCount >= KEY_DBL_INTERVAL) {
                            p->State = KEYSTATE_RELASE;
                            p->DblClkCount = 0;
                            if (p->ClickCount <= 1) {
                                //发送单击按键消息
                                KeyMessage_Put(p->KeyID, MYKEY_EVENT_CLICK, p->ClickCount);
                            } else {
                                //发送连击按键消息
                                KeyMessage_Put(p->KeyID, MYKEY_EVENT_DBLCLICK, p->ClickCount);
                            }
                            p->ClickCount = 0;
                        }
                    }
                    break;

                    //仅支持双击
                    case KEYSTATE_PRESS_D: {
                        p->DblClkCount += InterVal;
                        //超过时间没有双击
                        if (p->DblClkCount >= KEY_DBL_INTERVAL) {
                            p->State = KEYSTATE_RELASE;
                            p->DblClkCount = 0;
                            //发送连击消息
                            KeyMessage_Put(p->KeyID, MYKEY_EVENT_DBLCLICK, p->ClickCount);
                            p->ClickCount = 0;
                        }
                    }
                    break;

                    //仅支持单击
                    case KEYSTATE_PRESS_S: {
                        p->State = KEYSTATE_RELASE;
                        //即不支持长按，也不支持连续触发
                        if (!( ((p->Mode)&MYKEY_EVENT_LONG_PRESS) || ((p->Mode)&MYKEY_EVENT_REPEAT) )) {
                            //发送按键松开消息
                            KeyMessage_Put(p->KeyID, MYKEY_EVENT_RELASE, p->ClickCount);
                        } else {
                            //发送单击按键消息
                            KeyMessage_Put(p->KeyID, MYKEY_EVENT_CLICK, p->ClickCount);
                        }
                    }
                    break;

                    //支持长按和连续触发
                    case KEYSTATE_PRESS_LR:
                    //仅支持长按
                    case KEYSTATE_PRESS_L:
                    //仅支持连续触发
                    case KEYSTATE_PRESS_R: {
                        p->State = KEYSTATE_RELASE;
                        //发送按键松开消息
                        KeyMessage_Put(p->KeyID, MYKEY_EVENT_RELASE, p->ClickCount);
                    }
                    break;

                    default: {
                        p->State = KEYSTATE_RELASE;
                    }
                    break;
                }
            }
        }
        p = p->Next_Key;
    }
}
