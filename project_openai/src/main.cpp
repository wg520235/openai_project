#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include"mqttclient.h"
#include"chat.h"

int main(int argc, char *argv[])
{
	if(argc < 3){
		printf("please input your api key, exmple:./OpenAi_Server sk-bfLnibcPiR....Fw4ub2BFSJk8 \n");
		return 0;
	}
	

	MqttClient_Init(argv[1], argv[2], argv[2]);
	while(1)
	{
		sleep(1);
	}
	
	MqttClient_UnInit();
	return 0;
}
