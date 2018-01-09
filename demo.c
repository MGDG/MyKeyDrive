/**
  ******************************************************************************
  * @file    demo.c
  * @author  mgdg
  * @version V1.0.0
  * @date    2017-09-04
  * @brief   demo
  ******************************************************************************
 **/
#include "MyKeyDrive.h"

//声明两个按键
static MyKeyHandle key1,key2;


void __定时器中断或者周期性任务__(void) 
{ 
	size_t CycleTime = 中断时间或者任务周期，单位ms; 
	MyKey_KeyScan(CycleTime); 
} 


bool GetKeyStatus_key1(void) 
{ 
	//返回按键按下状态 
} 

 
bool GetKeyStatus_key2(void) 
{ 
	//返回按键按下状态 
} 


//注册按键
void KeyRegister(void)
{
	//注册按键1，支持单击和长按，长按时间为1000ms
	if(MyKey_KeyRegister(&key1,GetKeyStatus_key1,MYKEY_CLICK|MYKEY_LONG_PRESS,100,1000))
	//	if(MyKey_KeyRegister(&key1,GPIO_GetAcOn,MYKEY_CLICK|MYKEY_DBLCLICK|MYKEY_LONG_PRESS,100,1000))
		printf("key1 register successed\r\n");
	else
		printf("key1 register failed\r\n");

	//注册按键2，支持单击、长按和连续触发，长按时间为1000ms，连续触发间隔为100ms
	if(MyKey_KeyRegister(&key2,GetKeyStatus_key2,MYKEY_CLICK|MYKEY_DBLCLICK|MYKEY_LONG_PRESS|MYKEY_REPEAT,100,1000))
		printf("key2 register successed\r\n");
	else
		printf("key2 register failed\r\n");
}


//按键1事件处理函数
static void Key1Process(size_t KeyEvent,size_t KeyClickCount)
{
	switch(KeyEvent)
	{
		case MYKEY_CLICK:
		{
			//按键单击处理
			printf("key1 click, count %d\r\n",KeyClickCount);
		}
		break;

		case MYKEY_DBLCLICK:
		{
			//按键双击处理
			printf("key1 double click, count %d\r\n",KeyClickCount);

			//连续点击三次，卸载按键2
			if(KeyClickCount == 3)
			{
				if(MyKey_KeyUnregister(&key2))
					printf("key2 unregister success\r\n");
				else
					printf("key2 unregister failed\r\n");
			}
			//连续点击四次，注册按键2
			else if(KeyClickCount == 4)
			{
				if(MyKey_KeyRegister(&key2,GetKeyStatus_key2,MYKEY_CLICK|MYKEY_DBLCLICK|MYKEY_LONG_PRESS|MYKEY_REPEAT,100,1000))
					printf("key2 register successed\r\n");
				else
					printf("key2 register failed\r\n");
			}
			//连续点击5次，卸载按键1，注意，此处将自己卸载了，卸载之后这个按键就没反应了
			else if(KeyClickCount == 5)
			{
				if(MyKey_KeyUnregister(&key1))
					printf("key1 unregister success\r\n");
				else
					printf("key1 unregister failed\r\n");
			}

		}
		break;

		case MYKEY_LONG_PRESS:
		{
			//按键长按处理
			printf("key1 long press, count %d\r\n",KeyClickCount);

			//长按打印出已经注册的按键ID
			MyKey_GetKeyInfo();
		}
		break;

		case MYKEY_REPEAT:
		{
			//按键连续触发处理
			printf("key1 repeat, count %d\r\n",KeyClickCount);
		}
		break;
		
		case MYKEY_RELASE:
		{
			printf("key1 relase\r\n");
		}
		break;

		default:
			printf("undefine key message\r\n");
		break;
	}
}

//按键2事件处理函数
static void Key2Process(size_t KeyEvent,size_t KeyClickCount)
{
	switch(KeyEvent)
	{
		case MYKEY_CLICK:
		{
			//按键单击处理
			printf("key2 click, count %d\r\n",KeyClickCount);
		}
		break;

		case MYKEY_DBLCLICK:
		{
			//按键双击处理
			printf("key2 double click, count %d\r\n",KeyClickCount);
		}
		break;

		case MYKEY_LONG_PRESS:
		{
			//按键长按处理
			printf("key2 long press, count %d\r\n",KeyClickCount);
		}
		break;

		case MYKEY_REPEAT:
		{
			//按键连续触发处理
			printf("key2 repeat, count %d\r\n",KeyClickCount);
		}
		break;
		
		case MYKEY_RELASE:
		{
			printf("key2 relase\r\n");
		}
		break;

		default:
			printf("undefine key message\r\n");
		break;
	}
}

//主循环调用，从按键消息队列中取出消息并进行处理
void KeyProcess(void)
{
	MyKeyHandle KeyID;
	unsigned char KeyEvent;
	unsigned char KeyCliclCount;

	while(KeyMessage_Get(&KeyID,&KeyEvent,&KeyCliclCount))
	{
		if(KeyID == key1)
			Key1Process(KeyEvent,KeyCliclCount);
		else if(KeyID == key2)
			Key2Process(KeyEvent,KeyCliclCount);
	}
}



int main(void)
{
	KeyMessage_Init(5);					//初始化按键消息队列，队列深度5
	KeyRegister();

	while(1)
	{
		KeyProcess();
	}
}