#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include"mqttclient.h"

int main(int argc, char *argv[])
{
	if(argc < 3){
		printf("please input your api key, exmple:./OpenAi_Server sk-bfLnibcPiR....Fw4ub2BFSJk8 gpt_model dalle_model \n");
		return 0;
	}
	

	MqttClient_Init(argv[1], argv[2], argv[3]);
	while(1)
	{
		sleep(1);
	}
	
	MqttClient_UnInit();
	return 0;
}
