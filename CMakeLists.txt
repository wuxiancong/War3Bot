cmake_minimum_required(VERSION 3.16)
project(War3Bot VERSION 1.0.0 LANGUAGES CXX)

# =========================================================
# 1. 平台兼容性设置 (Ninja 路径)
# =========================================================
if(WIN32)
    if(EXISTS "D:/Qt/Tools/Ninja/ninja.exe")
        set(CMAKE_MAKE_PROGRAM "D:/Qt/Tools/Ninja/ninja.exe" CACHE FILEPATH "Program used to build from makefiles" FORCE)
    endif()
endif()

# =========================================================
# 2. 基础配置
# =========================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Network)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
if(WIN32)
    add_definitions(
        -DMOS_WINDOWS
        -DMUTIL_LIB_BUILD
        -D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    )
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -O2)
elseif(MSVC)
    # 1. 基础编译环境设置
    set(CMAKE_GENERATOR_PLATFORM Win32)

    # 2. 设置运行时库为动态链接 (/MD)
    # 移除所有其他运行时库的默认设置
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

    # 3. 编译选项
    # /MP: 启用多核编译 (加快编译速度)
    # /utf-8: 源码和执行字符集设为 UTF-8 (需 VS2015+)
    add_compile_options("/utf-8" "/MP")

    # 4. 设置链接器选项
    add_link_options("/NODEFAULTLIB:LIBCMT")
    add_link_options("/NODEFAULTLIB:LIBCMTD")
endif()

# =========================================================
# 3. 自动扫描源文件
# =========================================================

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h" "include/*.hpp")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
file(GLOB_RECURSE BNCS_FILES CONFIGURE_DEPENDS "bncsutil/*.h" "bncsutil/*.c" "bncsutil/*.cpp")

# 创建可执行文件
add_executable(War3Bot ${SOURCES} ${HEADERS} ${BNCS_FILES})

# =========================================================
# ★★★ 自动复制 war3files 和 config 到生成目录 ★★★
# =========================================================

# 1. 复制 war3files 文件夹 (如果源码目录下存在)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/war3files")
    add_custom_command(TARGET War3Bot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:War3Bot>/war3files"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/war3files"
        "$<TARGET_FILE_DIR:War3Bot>/war3files"
        COMMENT "正在将 war3files 目录复制到构建目录..."
    )
else()
    message(WARNING "在源码目录未找到 war3files 文件夹，请手动创建并放入 War3.exe, Storm.dll, Game.dll")
endif()

# 2. 复制 config 文件夹 (如果源码目录下存在)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
    add_custom_command(TARGET War3Bot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:War3Bot>/config"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:War3Bot>/config"
        COMMENT "正在将 config 目录复制到构建目录..."
    )
endif()

# =========================================================
# 4. GMP 库配置
# =========================================================

# 定义本地库的根路径
set(LOCAL_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib")

if(MSVC)
    # 指向我们刚刚创建并复制了文件的目录
    set(GMP_ROOT "${LOCAL_LIB_ROOT}/msvc/gmp")

    # 1. 检查文件是否存在
    if(NOT EXISTS "${GMP_ROOT}/include/gmp.h")
        message(FATAL_ERROR "未找到 GMP 头文件，请确保已将 gmp.h 复制到 ${GMP_ROOT}/include")
    endif()

    # 2. 包含头文件路径
    target_include_directories(War3Bot PRIVATE "${GMP_ROOT}/include")

    # 3. 链接 gmp.lib
    target_link_libraries(War3Bot PRIVATE "${GMP_ROOT}/lib/gmp.lib")

    # 4. 代码用了 C++ 接口，把 gmpxx 也链上
    if(EXISTS "${GMP_ROOT}/lib/gmpxx.lib")
        target_link_libraries(War3Bot PRIVATE "${GMP_ROOT}/lib/gmpxx.lib")
    endif()

    # 5. 编译后自动将 gmp.dll 复制到 .exe 旁边
    add_custom_command(TARGET War3Bot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GMP_ROOT}/bin/gmp.dll"
        "$<TARGET_FILE_DIR:War3Bot>/gmp.dll"
        COMMENT "正在将 gmp.dll 部署到运行目录..."
    )

elseif(MINGW)
    # MinGW 的原有配置
    set(GMP_ROOT "${LOCAL_LIB_ROOT}/mingw/gmp")
    target_include_directories(War3Bot PRIVATE "${GMP_ROOT}/include")
    target_link_libraries(War3Bot PRIVATE "${GMP_ROOT}/lib/libgmpxx.a" "${GMP_ROOT}/lib/libgmp.a")
endif()

# =========================================================
# 5. 公共链接配置
# =========================================================

target_include_directories(War3Bot PRIVATE include bncsutil) # 确保能include bncsutil
target_link_libraries(War3Bot PRIVATE
    Qt5::Core
    Qt5::Network
    ws2_32
    version
)

if(WIN32)
    target_link_libraries(War3Bot PRIVATE ws2_32)
endif()

# =========================================================
# 6. 安装配置
# =========================================================

install(TARGETS War3Bot DESTINATION bin)
