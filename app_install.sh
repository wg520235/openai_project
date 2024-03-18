#!/bin/bash

# 当前工作目录
CURRENT_DIR=$PWD
#服务运行脚本
cp $CURRENT_DIR/app_run.sh /usr/bin
chmod +x /usr/bin/app_run.sh
#工程路径
cd project_openai
#建立库文件和头文件目录
mkdir include lib build
cd ..
#安装相关编译工具
sudo apt install make gcc g++ cmake
sudo apt-get install libssl-dev

# 定义源码下载版本
CURL_VERSION="7.81.0"
MOSQUITTO_VERSION="1.6.9"

# 定义CMake工程的路径
CMAKE_PROJECT_CURL_PATH=$CURRENT_DIR"/project_openai/extralib/curl"
CMAKE_PROJECT_MQTT_PATH=$CURRENT_DIR"/project_openai/extralib/mqtt"
#服务器程序编译路径
CMAKE_PROJECT_BUILD_PATH=$CURRENT_DIR"/project_openai/build"
#服务器程序路径
CMAKE_PROJECT_EXEAPP_PATH=$CURRENT_DIR"/project_openai/bin"

# 创建临时编译目录
mkdir temp_build
cd temp_build
# 下载并编译Curl
echo "Downloading and compiling Curl..."
wget https://curl.se/download/curl-${CURL_VERSION}.tar.gz
tar -xvf curl-${CURL_VERSION}.tar.gz
cd curl-${CURL_VERSION}
./configure --prefix=${CMAKE_PROJECT_CURL_PATH} --with-ssl
make -j8
make install
cd ..


# 设置下载链接
MOSQUITTO_URL="https://mosquitto.org/files/source/mosquitto-$MOSQUITTO_VERSION.tar.gz"
# 设置安装目录
INSTALL_DIR=${CMAKE_PROJECT_MQTT_PATH}
# 下载Mosquitto源码
wget $MOSQUITTO_URL
# 解压源码包
tar -xzvf mosquitto-$MOSQUITTO_VERSION.tar.gz
# 检查utlist.h是否存在，如果不存在则下载
if [ ! -f /usr/local/include/utlist.h ]; then
  wget -O /usr/local/include/utlist.h https://raw.githubusercontent.com/troydhanson/uthash/master/src/utlist.h
fi
# 进入解压后的目录
cd mosquitto-$MOSQUITTO_VERSION

# 编译Mosquitto
# 你可以根据需要调整CMake参数
cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR .
make

# 安装Mosquitto
# 这会将必要的头文件和库文件安装到指定的_INSTALL目录
make install

echo "Mosquitto安装完成。头文件和库文件已经安装到 $INSTALL_DIR"


# 清理临时编译目录
cd ..
rm -rf temp_build

echo "All installations completed."

#编译服务程序
cd $CMAKE_PROJECT_BUILD_PATH
cmake ..
make

#开启mqtt服务器
cd $CURRENT_DIR
cp mosquitto.conf /etc/mosquitto/mosquitto.conf
mosquitto -c /etc/mosquitto/mosquitto.conf &
sleep 3
#后台运行服务程序
nohup app_run.sh $1 >/dev/null 2>&1 &

echo "All server start completed."
