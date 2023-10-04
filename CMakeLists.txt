# 设置CMake的最低版本要求
cmake_minimum_required(VERSION 3.14)

# 设置项目名称
project(Mini-Nginx)

# 指定C++标准
set(CMAKE_CXX_STANDARD 14)

# 设置 Release 模式
set(CMAKE_BUILD_TYPE Release)

# 添加执行文件
add_executable(Mini-Nginx
    main.cpp
    core/src/conf_parser.cpp
    http/src/http_parser.cpp
    http/src/http_proxy.cpp
)

# 添加包含目录
target_include_directories(Mini-Nginx
    PRIVATE
    core/include
    http/include
)