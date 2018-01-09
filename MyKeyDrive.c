/**
  ******************************************************************************
  * @file    MyKeyDrive.c
  * @author  mgdg
  * @version V1.0.0
  * @date    2017-09-04
  * @brief   注册一个按键，注册时选择按键支持的检测方式，如单击、双击、长按、连续触发、长按时间、连续触发间隔。
  *          按键消息用到了队列驱动
  ******************************************************************************
 **/
#include "MyKeyDrive.h"

		
static const size_t KEY_FILTER_TIME = 30;		//消抖滤波时间，单位ms
static const size_t KEY_DBL_INTERVAL = 250;		//双击最大间隔时间，单位ms

//按键状态
#define KEYSTATE_RELASE		(unsigned char)0	//按键松开
#define KEYSTATE_PRESS_SD	(unsigned char)1	//按键按下，支持单击和双击模式
#define KEYSTATE_PRESS_D	(unsigned char)2	//按键按下，支持双击模式
#define KEYSTATE_PRESS_S	(unsigned char)3	//按键按下，支持单击模式
#define KEYSTATE_PRESS_LR	(unsigned char)4	//按键按下，支持长按和连续触发模式
#define KEYSTATE_PRESS_L	(unsigned char)5	//按键按下，支持长按模式
#define KEYSTATE_PRESS_R	(unsigned char)6	//按键按下，支持连续触发模式

//按键消息结构体定义
typedef struct
{
	MyKeyHandle KeyID;							//按键ID
	unsigned char KeyEvent;						//按键事件
	unsigned char KeyClickCount;				//按键次数计数
}KeyMessage_Typedef;

static MyQueue_Typedef KeyBufQueue;

#if 0
struct
{
	KeyMessage_Typedef *queue;
	size_t len;						// 队列长度
	size_t front;					// 数据头,指向下一个空闲存放地址
	size_t rear;					// 数据尾，指向第一个数据
}KeyMessageQueue;
#endif

//按键属性
typedef struct KEY_NODE
{
	MyKeyHandle KeyID;						//按键对应的ID，每个按键对应唯一的ID
	KeyStatusFunc KeyStatus;				//按键按下的判断函数,1表示按下					//初始化时指定
	struct KEY_NODE* Next_Key;				//下一个按键
	size_t PressTime;						//按键按下持续时间（ms）
	size_t FilterCount;						//消抖滤波计时（ms）
	size_t RepeatSpeed;						//连续触发周期（ms）							//初始化时指定
	size_t RepeatCount;						//连续触发周期计时（ms）
	size_t LongPressTime;					//长按时间，超过该时间认为是长按（ms）			//初始化时指定
	size_t DblClkCount;						//双击间隔时间计时（ms）
	
	bool KeyLock;							//自锁信号
	unsigned char ClickCount;				//连按次数计数
	unsigned char Mode;						//按键支持的检测模式							//初始化时指定
	unsigned char State;					//按键当前状态
}MYKEY_LINK_NODE;
static MYKEY_LINK_NODE *MyKeyList = NULL;	//已注册的按键链表


/**
  * @brief	按键注册完后放入链表
  * @param	**ListHead：指向链表头指针的指针
  * @param	KeyID：按键ID
  *
  * @return	bool
  * @remark	链表头为空的话则创建新链表并更新链表头指针
  */
static bool KeyList_Put(MYKEY_LINK_NODE **ListHead,MyKeyHandle KeyID)
{
	MYKEY_LINK_NODE* p = NULL;
 	MYKEY_LINK_NODE* q = NULL;
	
	if(KeyID == NULL)				//加入的是空的定时器序号
		return false;

	if(*ListHead==NULL)				//链表为空
	{
		p = (MYKEY_LINK_NODE *)KeyID;
		p->KeyID = KeyID;
		p->Next_Key = NULL;
		*ListHead = p;
		return true;
	}

	q = *ListHead;
 	p = (*ListHead)->Next_Key;
 	while(p != NULL)
 	{
		if(q->KeyID == KeyID)				//链表中已经存在
			return true;
 		q = p;
 		p = p->Next_Key;
 	}
	
	//链表中不存在，链接进去
	p = (MYKEY_LINK_NODE *)KeyID;

	//初始化节点数据
	p->KeyID = KeyID;
	p->Next_Key = NULL;
	//加入链表
	q->Next_Key = p;			
	return true;
}


/**
  * @brief	初始化按键消息队列
  * @param	message_len: 消息队列的长度
  *
  * @return	bool 创建成功返回true，创建失败返回false
  * @remark	
  */
bool KeyMessage_Init(size_t message_len)
{
	KeyMessage_Typedef *p = NULL;
	
	p = (KeyMessage_Typedef *)malloc(message_len*sizeof(KeyMessage_Typedef));
	

	if(MyQueue_Create(&KeyBufQueue,p,message_len,sizeof(KeyMessage_Typedef)))
	{
		printf("Key message queue create success, queue len %d\r\n",message_len);
		return true;
	}
	else
	{
		printf("Key message queue create failed\r\n");
		return false;
	}

}



/**
  * @brief	将按键消息放入队列
  * @param	KeyID: 按键句柄
  * @param	KeyEvent: 按键事件，单击、双击还是长按等等
  * @param	KeyClickCount: 按键点击次数
  *
  * @return	bool 放入成功返回true，放入失败返回false
  * @remark	放入失败则表示按键队列已经满了了
  */
static bool KeyMessage_Put(MyKeyHandle KeyID,unsigned char KeyEvent,unsigned char ClickCount)
{
	KeyMessage_Typedef temp;
	
	temp.KeyID = KeyID;
	temp.KeyEvent = KeyEvent;
	temp.KeyClickCount = ClickCount;

	return (MyQueue_Put(&KeyBufQueue,&temp,1));
}


/**
  * @brief	从按键消息队列中获取一个按键消息
  * @param	*KeyID: 按键句柄
  * @param	*KeyEvent: 按键事件，单击、双击还是长按等等
  * @param	*KeyClickCount: 按键点击次数
  *
  * @return	bool 获取成功返回true，获取失败返回false
  * @remark	获取失败则表示按键队列中已经没有消息了
  */
bool KeyMessage_Get(MyKeyHandle *KeyID,unsigned char *KeyEvent,unsigned char *KeyClickCount)
{
	KeyMessage_Typedef temp;
	
	if(MyQueue_Get(&KeyBufQueue,&temp,1))
	{
		*KeyID = temp.KeyID;
		*KeyEvent = temp.KeyEvent;
		*KeyClickCount = temp.KeyClickCount;
		return true;
	}
	return false;
}


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
bool MyKey_KeyRegister(MyKeyHandle *Key,KeyStatusFunc func,unsigned char Mode,size_t RepeatSpeed,size_t LongPressTime)
{
	MYKEY_LINK_NODE *NewKey = NULL;
	
	//先检查按键是否已经被注册过了
	MYKEY_LINK_NODE *p = MyKeyList;
	while(p != NULL)
	{
		if(p->KeyStatus == func)
			return false;
		p = p->Next_Key;
	}

	NewKey = (MYKEY_LINK_NODE *)malloc(sizeof(MYKEY_LINK_NODE));
	if(NewKey != NULL)
	{
		NewKey->KeyStatus = func;
		NewKey->KeyID = (MyKeyHandle)NewKey;
		NewKey->Mode = Mode;
		NewKey->State = 0;
		NewKey->PressTime = 0;
		NewKey->FilterCount = 0;
		NewKey->RepeatSpeed = RepeatSpeed;
		NewKey->RepeatCount = 0;
		NewKey->LongPressTime = LongPressTime;
		NewKey->ClickCount = 0;
		NewKey->KeyLock = false;
		NewKey->DblClkCount = 0;
		NewKey->Next_Key = NULL; 

		//放入链表
		if(KeyList_Put(&MyKeyList,NewKey))
		{
			*Key = (MyKeyHandle)NewKey;
			return true;
		}
		else
		{
			free(NewKey);
			return false;
		}
	}
	else
		return false;
}


/**
  * @brief	卸载一个按键
  * @param	*Key: 按键句柄
  *
  * @return	bool: 按键卸载状态，true表示卸载成功，false表示卸载失败
  * @remark
  */
bool MyKey_KeyUnregister(MyKeyHandle *Key)
{
	MYKEY_LINK_NODE *TempNode = (MYKEY_LINK_NODE *)(*Key);
	MYKEY_LINK_NODE *p = NULL;
	MYKEY_LINK_NODE *q = MyKeyList;
	
	if(TempNode==NULL)				//无效指定节点
		return false;
	
	//查找是否存在
	while(q != NULL)
	{
		if(q->KeyID == *Key)
			break;
		q = q->Next_Key;
	}
	if(q == NULL)
		return false;
	q = MyKeyList;
	
	//存在
	if(MyKeyList==NULL)				//无效链表头节点
	{
		if(TempNode->KeyID == *Key)
		{
			free(TempNode);				//释放掉被删除的节点
			*Key = NULL;
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
		if(TempNode->KeyID == *Key)
		{
			free(TempNode);				//释放掉被删除的节点
			*Key = NULL;
			return true;
		}
		return false;
	}

	if(q->KeyID == *Key)
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
		*Key = NULL;
		return true;
	}
	return false;
}


/**
  * @brief	打印出已注册的按键ID
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
		printf("NO KEY\r\n");
	}
	while(q != NULL)
	{
		printf("KEY ID : %08X\r\n",q->KeyID);
		
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
void MyKey_KeyScan(size_t InterVal)
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
				if(!(p->KeyLock))
				{
					p->KeyLock = true;

					//第一次按下
					if(p->State == KEYSTATE_RELASE)
					{
						p->ClickCount = 1;
						p->DblClkCount = 0;			//双击等待时间清除
						p->PressTime = 0;			//长按计时复位
						p->RepeatCount = 0;			//连续触发计时复位
						
						//支持单击和双击
						if( ((p->Mode)&MYKEY_CLICK) && ((p->Mode)&MYKEY_DBLCLICK) )
						{
							p->State = KEYSTATE_PRESS_SD;
						}
						//仅支持单击
						else if((p->Mode)&MYKEY_CLICK)
						{
							p->State = KEYSTATE_PRESS_S;
							
							//即不支持长按，也不支持连续触发
							if(!( ((p->Mode)&MYKEY_LONG_PRESS) || ((p->Mode)&MYKEY_REPEAT) ))
							{
								//发送单击按键消息
								KeyMessage_Put(p->KeyID,MYKEY_CLICK,p->ClickCount);
							}
							
						}
						//仅支持双击
						else if((p->Mode)&MYKEY_DBLCLICK)
						{
							p->State = KEYSTATE_PRESS_D;
						}
					}
					//上一次为支持双击按下（支持单击和双击、仅支持双击）
					else if(p->State == KEYSTATE_PRESS_SD  || p->State == KEYSTATE_PRESS_D)
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
								
								p->State = KEYSTATE_PRESS_LR;
								//发送按键长按消息
								//KeyMessage_Put(p->KeyID,MYKEY_LONG_PRESS);
							}
						}
						else
						{
							p->RepeatCount += InterVal;
							if(p->RepeatCount >= p->RepeatSpeed)
							{
								p->RepeatCount = 0;

								p->State = KEYSTATE_PRESS_LR;
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
								p->State = KEYSTATE_PRESS_L;
								
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
							
							p->State = KEYSTATE_PRESS_R;

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
				p->KeyLock = false;					//解锁

				switch(p->State)
				{
					//支持单击和双击
					case KEYSTATE_PRESS_SD:
					{
						p->DblClkCount += InterVal;

						//超过时间没有双击
						if(p->DblClkCount >= KEY_DBL_INTERVAL)
						{
							p->State = KEYSTATE_RELASE;
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
					case KEYSTATE_PRESS_D:
					{
						p->DblClkCount += InterVal;

						//超过时间没有双击
						if(p->DblClkCount >= KEY_DBL_INTERVAL)
						{
							p->State = KEYSTATE_RELASE;
							p->DblClkCount = 0;
							
							//发送连击消息
							KeyMessage_Put(p->KeyID,MYKEY_DBLCLICK,p->ClickCount);
							
							p->ClickCount = 0;
						}
					}
					break;
					
					//仅支持单击
					case KEYSTATE_PRESS_S:
					{
						p->State = KEYSTATE_RELASE;
					
						//发送单击按键消息
						//KeyMessage_Put(p->KeyID,MYKEY_CLICK,p->ClickCount);
						//即不支持长按，也不支持连续触发
						if(!( ((p->Mode)&MYKEY_LONG_PRESS) || ((p->Mode)&MYKEY_REPEAT) ))
						{
							//发送按键松开消息
							KeyMessage_Put(p->KeyID,MYKEY_RELASE,p->ClickCount);
						}
						else
						{
							//发送单击按键消息
							KeyMessage_Put(p->KeyID,MYKEY_CLICK,p->ClickCount);
						}
					}
					break;
					
					//支持长按和连续触发
					case KEYSTATE_PRESS_LR:
					//仅支持长按
					case KEYSTATE_PRESS_L:
					//仅支持连续触发
					case KEYSTATE_PRESS_R:
					{
						p->State = KEYSTATE_RELASE;
						
						//发送按键松开消息
						KeyMessage_Put(p->KeyID,MYKEY_RELASE,p->ClickCount);
					}
					break;

					
					default:
					{
						p->State = KEYSTATE_RELASE;
					}
					break;
				}
			}
		}

		p = p->Next_Key;
	}
}

