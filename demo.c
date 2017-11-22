/**
  ******************************************************************************
  * @file    demo.c
  * @author  mgdg
  * @version V1.0.0
  * @date    2017-09-04
  * @brief   
  ******************************************************************************
 **/
uint32_t key1,key2,key3;




void __定时器中断或者周期性任务__(void)
{
	uint32_t CycleTime = 中断时间或者任务周期，单位ms;
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

bool GetKeyStatus_key3(void)
{
	//返回按键按下状态
}

void KeyRegister(void)
{
	//注册按键1，支持单击、双击和长按，长按时间为1000ms
	key1 = MyKey_KeyRegister(GetKeyStatus_key1,MYKEY_CLICK|MYKEY_DBLCLICK|MYKEY_LONG_PRESS,100,1000);
	if(key1 == NULL)
		printf("key1 register failed\r\n");
	else
		printf("key1 register successed\r\n");

	//注册按键2，支持单击、长按和连续触发，长按时间为1000ms，连续触发间隔为100ms
	key2 = MyKey_KeyRegister(GetKeyStatus_key2,MYKEY_CLICK|MYKEY_REPEAT|MYKEY_LONG_PRESS,100,1000);
	if(key2 == NULL)
		printf("key2 register failed\r\n");
	else
		printf("key2 register successed\r\n");

	//注册按键3，支持单击和连续触发，连续触发间隔为200ms
	key3 = MyKey_KeyRegister(GetKeyStatus_key3,MYKEY_CLICK|MYKEY_REPEAT,200,1000);
	if(key3 == NULL)
		printf("key3 register failed\r\n");
	else
		printf("key3 register successed\r\n");
}

void Key1Process(uint32_t KeyEvent)
{
	switch(KeyEvent)
	{
		case MYKEY_CLICK:
		{
			//按键单击处理
			printf("key1 click\r\n");
		}
		break;

		case MYKEY_DBLCLICK:
		{
			//按键双击处理
			printf("key1 double click\r\n");
		}
		break;

		case MYKEY_LONG_PRESS:
		{
			//按键长按处理
			printf("key1 long press\r\n");
		}
		break;

		case MYKEY_REPEAT:
		{
			//按键连续触发处理
			printf("key1 repeat\r\n");
		}
		break;

		default:
			printf("undefine key message\r\n");
		break;
	}
}
void KeyProcess(void)
{
	uint32_t KeyID;
	uint8_t KeyEvent;

	if(KeyMessage_Get(&KeyID,&KeyEvent))
	{
		if(KeyID == key1)
			Key1Process(KeyEvent);
		else if(KeyID == key2)
			Key2Process(KeyEvent);
	}
}

int main(void)
{
	//初始化按键消息队列，队列深度5
	KeyMessage_Init(5);

	//注册按键
	KeyRegister();


	while(1)
	{
		//按键处理
		KeyProcess();
	}
	
}