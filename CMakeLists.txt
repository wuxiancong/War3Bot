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
        -NOMINMAX
        -DMOS_WINDOWS
        -DMUTIL_LIB_BUILD
        -DWIN32_LEAN_AND_MEAN
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    )
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -O2)
elseif(MSVC)
    # 1. 基础编译环境设置
    set(CMAKE_GENERATOR_PLATFORM Win32)

    # 2. 设置运行时库为动态链接 (/MD)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

    # 3. 编译选项
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

# 1. 复制 war3files 文件夹
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

# 2. 复制 config 文件夹
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
# 4. 外部库配置 (GMP & StormLib)
# =========================================================

# 定义本地库的根路径 (对应 D:\Qt_\War3Bot\lib)
set(LOCAL_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib")

# ---------------------------------------------------------
# 4.1 GMP 配置
# ---------------------------------------------------------
if(MSVC)
    set(GMP_ROOT "${LOCAL_LIB_ROOT}/msvc/gmp")

    if(NOT EXISTS "${GMP_ROOT}/include/gmp.h")
        message(FATAL_ERROR "未找到 GMP 头文件: ${GMP_ROOT}/include/gmp.h")
    endif()

    target_include_directories(War3Bot PRIVATE "${GMP_ROOT}/include")
    target_link_libraries(War3Bot PRIVATE "${GMP_ROOT}/lib/gmp.lib")

    if(EXISTS "${GMP_ROOT}/lib/gmpxx.lib")
        target_link_libraries(War3Bot PRIVATE "${GMP_ROOT}/lib/gmpxx.lib")
    endif()

    add_custom_command(TARGET War3Bot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GMP_ROOT}/bin/gmp-10.dll"
        "$<TARGET_FILE_DIR:War3Bot>/gmp-10.dll"
        COMMENT "正在部署 gmp-10.dll..."
    )
    if(EXISTS "${GMP_ROOT}/bin/gmpxx-4.dll")
        add_custom_command(TARGET War3Bot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GMP_ROOT}/bin/gmpxx-4.dll"
            "$<TARGET_FILE_DIR:War3Bot>/gmpxx-4.dll"
            COMMENT "正在部署 gmpxx-4.dll..."
        )
    endif()

elseif(MINGW)
    set(GMP_ROOT "${LOCAL_LIB_ROOT}/mingw/gmp")
    target_include_directories(War3Bot PRIVATE "${GMP_ROOT}/include")
    target_link_libraries(War3Bot PRIVATE "${GMP_ROOT}/lib/libgmpxx.a" "${GMP_ROOT}/lib/libgmp.a")
endif()

# ---------------------------------------------------------
# 4.2 StormLib 配置
# ---------------------------------------------------------
if(MSVC)
    set(STORMLIB_ROOT "${LOCAL_LIB_ROOT}/msvc/stormLib")

    target_include_directories(War3Bot PRIVATE "${STORMLIB_ROOT}/include")

    target_link_libraries(War3Bot PRIVATE
        "${STORMLIB_ROOT}/lib/StormLib.lib"
        "${STORMLIB_ROOT}/lib/zlib.lib"
        "${STORMLIB_ROOT}/lib/bz2.lib"
    )

    # 之前添加的防自动链接宏
    target_compile_definitions(War3Bot PRIVATE STORMLIB_NO_AUTO_LINK_LIB)

elseif(MINGW)
    set(STORMLIB_ROOT "${LOCAL_LIB_ROOT}/mingw/stormLib")
    target_include_directories(War3Bot PRIVATE "${STORMLIB_ROOT}/include")
    target_link_libraries(War3Bot PRIVATE
        "${STORMLIB_ROOT}/lib/libStormLib.a"
        "${STORMLIB_ROOT}/lib/libzlib.a"
        "${STORMLIB_ROOT}/lib/libbz2.a"
    )
endif()

# =========================================================
# 5. 公共链接配置
# =========================================================

target_include_directories(War3Bot PRIVATE include bncsutil)

# 链接库列表
target_link_libraries(War3Bot PRIVATE
    Qt5::Core
    Qt5::Network
    ws2_32
    version
    wininet  # 新增: StormLib 通常需要 wininet (Windows Internet API)
)

if(WIN32)
    target_link_libraries(War3Bot PRIVATE ws2_32)
endif()

# =========================================================
# 6. 安装配置
# =========================================================

install(TARGETS War3Bot DESTINATION bin)
