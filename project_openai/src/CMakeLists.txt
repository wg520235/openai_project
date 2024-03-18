
#########################################
#概要：项目工程代码编译脚本 创建者：Nik_TellYou 时间：2023年10月1日
#########################################
#将编译输出的可执行文件命名为main
SET(PROG_NAME OpenAI_server)
#指定工程编译所引用到的头文件的路径
include_directories ( 
		${PROJECT_SOURCE_DIR}/extralib/curl/include/curl
		${PROJECT_SOURCE_DIR}/extralib/mqtt/include 

		${PROJECT_SOURCE_DIR}/modules/cJson
		${PROJECT_SOURCE_DIR}/modules/mqttclient
		)
					
#将encode模块的源文件路径名称, 重命名为ENCODE_SRC
set(CJSON_SRC	${PROJECT_SOURCE_DIR}/modules/cJson/cJSON.c)
set(MQTTCLIENT_SRC ${PROJECT_SOURCE_DIR}/modules/mqttclient/mqttclient.cpp)
#将src目录下的所有.c文件名称, 保存到变量SRC_LIST
aux_source_directory (. SRC_LIST )
#指定工程编译所用到的库文件的路径
link_directories(
		${PROJECT_SOURCE_DIR}/extralib/curl/lib
		${PROJECT_SOURCE_DIR}/extralib/mqtt/lib 

		
		)		
#生成可以执行文件main
add_executable (${PROG_NAME} ${SRC_LIST}
		${CJSON_SRC}
		${MQTTCLIENT_SRC} 
		)

#链接可执行文件所需要的库文件
target_link_libraries (${PROG_NAME}
		#mqtt lib
		-lmosquitto
		#curl lib       
		-lcurl
		
		-lc -lm -lpthread -lrt -ldl )
		
if(CMAKE_COMPILER_IS_GNUCXX) 
	set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}") 
	message(STATUS "optional:-std=c++11")
endif(CMAKE_COMPILER_IS_GNUCXX)

#将生成的可执行文件输出保存到bin目录下
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
