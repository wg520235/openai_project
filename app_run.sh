#!/bin/bash
# 当前工作目录
CURRENT_DIR=$PWD
#服务运行脚本
echo $CURRENT_DIR
while true; do
	# 尝试运行你的程序并获取其PID
	/home/NikTalk_OpenAI_ChatGPT/project_openai/bin/OpenAI_server $1 &
	echo $1
	PID=$!
	echo $PID
	# 等待程序退出
	wait $PID
	
	# 获取当前系统时间
	current_time=$(date)
	# 输出时间到log.txt文件
	echo "mqtt_client exited!Current Time: $current_time" >> $current_time+log.txt
	
	# 稍作等待后重启
    sleep 5
done
