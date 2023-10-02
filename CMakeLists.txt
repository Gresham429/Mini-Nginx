# 设置CMake的最低版本要求
cmake_minimum_required(VERSION 3.14)

# 设置项目名称
project(Mini-Nginx)

# 指定C++标准
set(CMAKE_CXX_STANDARD 14)

# 添加其他编译选项，如优化等
# target_compile_options(Mini-Nginx PRIVATE -O2)

# 添加执行文件
add_executable(Mini-Nginx
    main.cpp
    http/src/http_parser.cpp
)

# 添加包含目录
target_include_directories(Mini-Nginx
    PRIVATE
    core/include
    http/include
)