cmake_minimum_required(VERSION 3.10)
project(RPCService VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找Python3
find_package(Python3 REQUIRED)

# 配置文件和生成的代码目录
set(YAML_CONFIG_FILE ${CMAKE_SOURCE_DIR}/rpc_config.yaml)
set(PYTHON_SCRIPT ${CMAKE_SOURCE_DIR}/builder.py)
set(GENERATED_CODE_DIR ${CMAKE_BINARY_DIR}/generated)

# 确保生成的代码目录存在
file(MAKE_DIRECTORY ${GENERATED_CODE_DIR})

# 从YAML文件中提取服务名（用于确定生成的文件名）
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "
import yaml, sys
with open('${YAML_CONFIG_FILE}', 'r') as f:
    config = yaml.safe_load(f)
print(config.get('service', 'DefaultService'))"
    OUTPUT_VARIABLE SERVICE_NAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# 计算生成的文件名
set(GENERATED_CLIENT_HEADER ${GENERATED_CODE_DIR}/${SERVICE_NAME}Client.h)
set(GENERATED_CLIENT_CPP ${GENERATED_CODE_DIR}/${SERVICE_NAME}Client.cpp)
set(GENERATED_SERVER_HEADER ${GENERATED_CODE_DIR}/${SERVICE_NAME}Server.h)
set(GENERATED_SERVER_CPP ${GENERATED_CODE_DIR}/${SERVICE_NAME}Server.cpp)

# 自定义命令：当YAML文件或Python脚本变化时，重新生成RPC代码
add_custom_command(
    OUTPUT
        ${GENERATED_CLIENT_HEADER}
        ${GENERATED_CLIENT_CPP}
        ${GENERATED_SERVER_HEADER}
        ${GENERATED_SERVER_CPP}
    COMMAND ${Python3_EXECUTABLE}
        ${PYTHON_SCRIPT}
        -i ${YAML_CONFIG_FILE}
        -o ${GENERATED_CODE_DIR}
        --verbose
    DEPENDS
        ${YAML_CONFIG_FILE}
        ${PYTHON_SCRIPT}
    COMMENT "Generating RPC code from ${YAML_CONFIG_FILE}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# 自定义目标：便于手动触发代码生成
add_custom_target(
    generate_rpc_code
    DEPENDS
        ${GENERATED_CLIENT_HEADER}
        ${GENERATED_CLIENT_CPP}
        ${GENERATED_SERVER_HEADER}
        ${GENERATED_SERVER_CPP}
    COMMENT "RPC code generation target"
)

# 主可执行文件
add_executable(${PROJECT_NAME}
    main_testcase.cpp
    ${GENERATED_CLIENT_CPP}
    ${GENERATED_SERVER_CPP}
)

# 设置目标属性
target_include_directories(${PROJECT_NAME} PRIVATE ${GENERATED_CODE_DIR})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)

# 添加依赖关系：主可执行文件依赖于生成的代码
add_dependencies(${PROJECT_NAME} generate_rpc_code)

# 如果有其他源文件，也可以在这里添加
# add_library(mylib STATIC other_source.cpp)

# 安装目标（可选）
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# 打印配置信息
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Service name: ${SERVICE_NAME}")
message(STATUS "Generated code directory: ${GENERATED_CODE_DIR}")
message(STATUS "Python executable: ${Python3_EXECUTABLE}")
message(STATUS "YAML config file: ${YAML_CONFIG_FILE}")