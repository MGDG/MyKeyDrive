/**
  ******************************************************************************
  * @file    MyKeyDrive.c
  * @author  mgdg
  * @version V1.0.0
  * @date    2017-09-04
  * @brief   注册一个按键，注册时选择按键支持的检测方式，如单击、连击、长按、连续触发、长按时间、连续触发间隔
  *          同时使能长按功能和连续触发功能后，则在长按时间到达之后不发送长按消息同时开始连续触发
  ******************************************************************************
 **/
#include "MyKeyDrive.h"
#include "MyQueue.h"

//按键消息结构体定义
typedef struct
{
	uint32_t KeyID;					//按键ID
	uint8_t KeyEvent;				//按键事件
	uint8_t KeyClickCount;			//按键次数计数
}KeyMessage_Typedef;

static MyQueue_Typedef KeyBufQueue;


//按键属性结构体
typedef struct KEY_NODE
{
	KeyStatusFunc KeyStatus;				//按键按下的判断函数,1表示按下					//初始化时指定
	uint32_t KeyID;							//按键对应的ID，每个按键对应唯一的ID
	struct KEY_NODE* Next_Key;				//下一个按键
	uint32_t PressTime;						//按键按下持续时间
	uint32_t FilterCount;					//消抖滤波计时
	uint32_t RepeatSpeed;					//连续触发周期								//初始化时指定
	uint32_t RepeatCount;					//连续触发周期计时
	uint32_t LongPressTime;					//长按时间，超过该时间认为是长按				//初始化时指定
	uint32_t DblClkCount;					//连击间隔时间计时
	
	uint8_t ClickCount;						//连按次数计数
	uint8_t Lock;							//自锁信号
	uint8_t Mode;							//按键支持的检测模式							//初始化时指定
	uint8_t State;							//按键当前状态
}MYKEY_LINK_NODE;
static MYKEY_LINK_NODE *MyKeyList = NULL;	//已注册的按键链表


/**
  * @brief	按键注册完后放入列表
  * @param	**ListHead：指向链表头指针的指针
  * @param	KeyID：按键ID
  *
  * @return	bool
  * @remark	链表头为空的话则创建新链表并更新链表头指针
  */
static bool KeyList_Put(MYKEY_LINK_NODE **ListHead,MYKEY_LINK_NODE *KeyID)
{
	MYKEY_LINK_NODE* p = NULL;
 	MYKEY_LINK_NODE* q = NULL;
	
	if(KeyID == NULL)				//加入的是空的按键ID
		return false;

	if(*ListHead==NULL)				//链表为空
	{
		p = (MYKEY_LINK_NODE *)KeyID;
		p->KeyID = (uint32_t)KeyID;
		p->Next_Key = NULL;
		*ListHead = p;
		return true;
	}

	q = *ListHead;
 	p = (*ListHead)->Next_Key;
 	while(p != NULL)
 	{
		if(q->KeyID == (uint32_t)KeyID)				//链表中已经存在
			return true;
 		q = p;
 		p = p->Next_Key;
 	}
	
	//链表中不存在，链接进去
	p = (MYKEY_LINK_NODE *)KeyID;

	//初始化节点数据
	p->KeyID = (uint32_t)KeyID;
	p->Next_Key = NULL;
	//加入链表
	q->Next_Key = p;			
	return true;
}


/**
  * @brief	初始化按键消息队列
  * @param	message_len：消息队列的长度
  *
  * @return	void
  * @remark 使用开辟内存的方式创建的队列，注意不要太长容易导致堆栈溢出
  */
void KeyMessage_Init(uint8_t message_len)
{
	KeyMessage_Typedef *p = NULL;
	
	p = (KeyMessage_Typedef *)malloc(message_len*sizeof(KeyMessage_Typedef));
	
	if(MyQueue_Create(&KeyBufQueue,p,message_len,sizeof(KeyMessage_Typedef)))
		printf("Key message queue create success");
	else
		printf("Key message queue create failed");
}


/**
  * @brief	将按键消息放入队列
  * @param	KeyID:当前触发的按键ID
  * @param	KeyEvent:按键的事件类型，单击、连击或者长按等等
  * @param	ClickCount:按键的点击次数
  *
  * @return	bool
  * @remark
  */
static bool KeyMessage_Put(uint32_t KeyID,uint8_t KeyEvent,uint8_t ClickCount)
{
	KeyMessage_Typedef temp;
	
	temp.KeyID = KeyID;
	temp.KeyEvent = KeyEvent;
	temp.KeyClickCount = ClickCount;
	
	return MyQueue_Put(&KeyBufQueue,&temp);
}


/**
  * @brief	从队列中获取一个按键消息并弹出
  * @param	*KeyID:当前触发的按键ID
  * @param	*KeyEvent:按键的事件类型，单击、连击或者长按等等
  * @param	*ClickCount:按键的点击次数
  *
  * @return	bool
  * @remark
  */
bool KeyMessage_Get(uint32_t *KeyID,uint8_t *KeyEvent,uint8_t *KeyClickCount)
{
	KeyMessage_Typedef temp;
	
	if(MyQueue_Get(&KeyBufQueue,&temp))
	{
		*KeyID = temp.KeyID;
		*KeyEvent = temp.KeyEvent;
		*KeyClickCount = temp.KeyClickCount;
		return true;
	}
	return false;
}


/**
  * @brief	从消息队列中获取一个按键但不弹出
  * @param	*KeyID:当前触发的按键ID
  * @param	*KeyEvent:按键的事件类型，单击、连击或者长按等等
  * @param	*ClickCount:按键的点击次数
  *
  * @return	bool
  * @remark
  */
bool KeyMessage_Peek(uint32_t *KeyID,uint8_t *KeyEvent,uint8_t *KeyClickCount)
{
	KeyMessage_Typedef temp;
	
	if(MyQueue_Peek(&KeyBufQueue,&temp))
	{
		*KeyID = temp.KeyID;
		*KeyEvent = temp.KeyEvent;
		*KeyClickCount = temp.KeyClickCount;
		return true;
	}
	return false;
}


/**
  * @brief	注册一个按键，返回注册到的按键ID，返回0表示注册失败
  * @param	func：按键检测函数
  * @param	Mode：按键支持的检测方式，如单击、连击等，可以同时使能，如 MYKEY_CLICK|MYKEY_DBLCLICK 表示同时使能单击和连击检测
  * @param	RepeatSpeed：使能连续触发功能后，连续触发的时间间隔，单位ms
  * @param	LongPressTime：长按时间，按住按键操作该时间则发送长按事件，单位ms
  *
  * @return	uint32_t:按键ID
  * @remark	返回注册到的按键ID，返回0表示注册失败
  */
uint32_t MyKey_KeyRegister(KeyStatusFunc func,uint8_t Mode,uint32_t RepeatSpeed,uint32_t LongPressTime)
{
	MYKEY_LINK_NODE *NewKey = NULL;

	//先检查按键是否已经被注册过了
	MYKEY_LINK_NODE *p = MyKeyList;
	while(p != NULL)
	{
		if(p->KeyStatus == func)
			return NULL;
		p = p->Next_Key;
	}


	NewKey = (MYKEY_LINK_NODE *)malloc(sizeof(MYKEY_LINK_NODE));
	if(NewKey != NULL)
	{
		NewKey->KeyStatus = func;
		NewKey->KeyID = (uint32_t)NewKey;
		NewKey->Mode = Mode;
		NewKey->State = 0;
		NewKey->PressTime = 0;
		NewKey->FilterCount = 0;
		NewKey->RepeatSpeed = RepeatSpeed;
		NewKey->RepeatCount = 0;
		NewKey->LongPressTime = LongPressTime;
		NewKey->Lock = 0;
		NewKey->DblClkCount = 0;
		NewKey->Next_Key = NULL; 

		//放入链表
		if(KeyList_Put(&MyKeyList,NewKey))
			return (uint32_t)NewKey;
		else
		{
			free(NewKey);
			return NULL;
		}
	}
	else
		return NULL;
}


/**
  * @brief	卸载一个按键
  * @param	KeyID，按键ID
  *
  * @return	bool
  * @remark	返回0表示卸载失败
  */
bool MyKey_KeyUnregister(int32_t KeyID)
{
	MYKEY_LINK_NODE *TempNode = (MYKEY_LINK_NODE *)KeyID;
	MYKEY_LINK_NODE *p = NULL;
	MYKEY_LINK_NODE *q = MyKeyList;
	
	if(TempNode==NULL)				//无效指定节点
		return false;
	
	//查找是否存在
	while(q != NULL)
	{
		if(q->KeyID == KeyID)
			break;
		q = q->Next_Key;
	}
	if(q == NULL)
		return false;
	q = MyKeyList;
	
	//存在
	if(MyKeyList==NULL)				//无效链表头节点
	{
		if(TempNode->KeyID == KeyID)
		{
			free(TempNode);				//释放掉被删除的节点
			return true;
		}
		return false;
	}
		
	while(q != TempNode && q != NULL)
	{
		p = q;
		q = q->Next_Key;
	}

	if(q != TempNode)				//链表中不存在该节点
	{
		if(TempNode->KeyID == KeyID)
		{
			free(TempNode);				//释放掉被删除的节点
			return true;
		}
		return false;
	}

	if(q->KeyID == KeyID)
	{
		if(q==MyKeyList)				//删除的是第一个节点
		{
			MyKeyList = MyKeyList->Next_Key;				
		}
		else
		{
			p->Next_Key = q->Next_Key;
		}
		free(q);						//释放掉被删除的节点
		return true;
	}
	return false;
}


/**
  * @brief	打印出所有按键的ID
  * @param	void
  *
  * @return	void
  * @remark
  */
void MyKey_GetKeyInfo(void)
{
	MYKEY_LINK_NODE* q = MyKeyList;
	
	if(q == NULL)
	{
		printf("NO KEY");
	}
	while(q != NULL)
	{
		printf("KEY ID : %08X",q->KeyID);
		
		q= q->Next_Key;
	}
}


/**
  * @brief	按键扫描,需要周期调用
  * @param	InterVal，调用间隔，单位ms
  *
  * @return	void
  * @remark	周期调用
  */
void MyKey_KeyScan(uint32_t InterVal)
{
	MYKEY_LINK_NODE *p = MyKeyList;

	while(p != NULL)
	{
		//按键检测到有效电平
		if(p->KeyStatus())
		{
			//按下消抖
			if(p->FilterCount < KEY_FILTER_TIME)
			{
				p->FilterCount = KEY_FILTER_TIME;
			}
			else if(p->FilterCount < (KEY_FILTER_TIME+KEY_FILTER_TIME))
			{
				p->FilterCount += InterVal;
			}
			//消抖时间已到
			else
			{
				//上一次状态为弹起
				if(!(p->Lock))
				{
					p->Lock = 1;

					//第一次按下
					if(p->State == 0)
					{
						p->ClickCount = 1;
						p->DblClkCount = 0;			//双击等待时间清除
						p->PressTime = 0;			//长按计时复位
						p->RepeatCount = 0;			//连续触发计时复位
						
						//支持单击和双击
						if( ((p->Mode)&MYKEY_CLICK) && ((p->Mode)&MYKEY_DBLCLICK) )
						{
							p->State = 1;
						}
						//仅支持单击
						else if((p->Mode)&MYKEY_CLICK)
						{
							p->State = 3;
						}
						//仅支持双击
						else if((p->Mode)&MYKEY_DBLCLICK)
						{
							p->State = 2;
						}
					}
					//上一次为支持连击按下（支持单击和连击、仅支持连击）
					else if(p->State == 2  || p->State == 1)
					{
						p->DblClkCount = 0;						//连击间隔时间清0

						//连续按次数加1
						if(p->ClickCount < 255)
							p->ClickCount++;
					}
				}
				//持续按住
				else
				{
					//同时使能长按和连续触发，长按时间到达之后开始连续触发，不发送长按消息
					if( ((p->Mode)&MYKEY_LONG_PRESS) && ((p->Mode)&MYKEY_REPEAT) )
					{
						if(p->PressTime < p->LongPressTime)
						{
							p->PressTime += InterVal;

							if(p->PressTime >= p->LongPressTime)
							{
								p->RepeatCount = 0;				//重复触发计时清0
								
								p->State = 0;
							}
						}
						else
						{
							p->RepeatCount += InterVal;
							if(p->RepeatCount >= p->RepeatSpeed)
							{
								p->RepeatCount = 0;

								p->State = 0;
								//发送连续按键消息
								KeyMessage_Put(p->KeyID,MYKEY_REPEAT,p->ClickCount);
								
								if(p->ClickCount < 255)
									p->ClickCount++;
							}
						}
					}
					//只使能长按功能
					else if((p->Mode)&MYKEY_LONG_PRESS)
					{
						if(p->PressTime < p->LongPressTime)
						{
							p->PressTime += InterVal;
							if(p->PressTime >= p->LongPressTime)
							{
								p->State = 0;
								
								//发送按键长按消息
								KeyMessage_Put(p->KeyID,MYKEY_LONG_PRESS,p->ClickCount);
							}
						}
					}
					//只使能连发功能
					else if((p->Mode)&MYKEY_REPEAT)
					{
						p->RepeatCount += InterVal;

						if(p->RepeatCount >= p->RepeatSpeed)
						{
							p->RepeatCount = 0;
							
							p->State = 0;

							//发送连续按键消息
							KeyMessage_Put(p->KeyID,MYKEY_REPEAT,p->ClickCount);
							
							if(p->ClickCount < 255)
								p->ClickCount++;
						}
					}
				}
			}
		}
		else
		{
			//弹起消抖
			if(p->FilterCount > KEY_FILTER_TIME)
			{
				p->FilterCount = KEY_FILTER_TIME;
			}
			else if(p->FilterCount != 0)
			{
				if(p->FilterCount >= InterVal)
					p->FilterCount -= InterVal;
				else
					p->FilterCount = 0;
			}
			//消抖时间到
			else
			{
				p->Lock = 0;					//解锁

				switch(p->State)
				{
					//支持单击和双击
					case 1:
					{
						p->DblClkCount += InterVal;

						//超过时间没有双击
						if(p->DblClkCount >= KEY_DBL_INTERVAL)
						{
							p->State = 0;
							p->DblClkCount = 0;
							
							if(p->ClickCount <= 1)
							{
								//发送单击按键消息
								KeyMessage_Put(p->KeyID,MYKEY_CLICK,p->ClickCount);
							}
							else
							{
								//发送连击按键消息
								KeyMessage_Put(p->KeyID,MYKEY_DBLCLICK,p->ClickCount);
							}
							
							p->ClickCount = 0;
						}
					}
					break;
					
					//仅支持双击
					case 2:
					{
						p->DblClkCount += InterVal;

						//超过时间没有双击
						if(p->DblClkCount >= KEY_DBL_INTERVAL)
						{
							p->State = 0;
							p->DblClkCount = 0;
							
							//发送连击消息
							KeyMessage_Put(p->KeyID,MYKEY_DBLCLICK,p->ClickCount);
							
							p->ClickCount = 0;
						}
					}
					break;
					
					//仅支持单击
					case 3:
					{
						p->State = 0;
					
						//发送单击按键消息
						KeyMessage_Put(p->KeyID,MYKEY_CLICK,p->ClickCount);
					}
					break;
					
					default:
					{
						p->State = 0;
					}
					break;
				}
			}
		}

		p = p->Next_Key;
	}
}


