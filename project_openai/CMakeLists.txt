cmake_minimum_required (VERSION 3.5.1)#cmake 的最低版本

project (OpenAI_Server VERSION 1.0)#项目的名称

set (CMAKE_C_COMPILER "gcc")#指定编译c代码的编译器
set (CMAKE_CXX_COMPILER "g++")#指定编译c++代码的编译器

#2、再执行src目录下的CMakeLists.txt脚本
add_subdirectory (src)
