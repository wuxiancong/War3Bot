cmake_minimum_required(VERSION 3.16)

# =========================================================
# 1. 核心环境配置
# =========================================================

# 将 Windows 特有的硬编码配置限制在 WIN32 范围内
if(WIN32)
    # 1.1 强制指定 Windows SDK 版本
    set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "Windows SDK Version" FORCE)

    # 1.2 指定 Qt5 路径
    if(MSVC)
        # 1.3 指定 Ninja (仅在 Windows 下强制指定)
        set(CMAKE_MAKE_PROGRAM "D:/Qt/Tools/Ninja/ninja.exe" CACHE FILEPATH "Program used to build from makefiles" FORCE)

        # 1.4 指定编译器工具 (rc 和 mt)
        set(CMAKE_RC_COMPILER "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x86/rc.exe" CACHE FILEPATH "Resource Compiler" FORCE)
        set(CMAKE_MT "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x86/mt.exe" CACHE FILEPATH "Manifest Tool" FORCE)
    endif()
endif()

# 1.5 设置默认构建类型为 Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS ">> 未检测到构建类型，强制默认为: Release")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# =========================================================
# 2. 基础配置
# =========================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(War3Bot VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt5 REQUIRED COMPONENTS Core Network)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows 特有宏定义
if(WIN32)
    add_definitions(
        -DMOS_WINDOWS
        -DMUTIL_LIB_BUILD
        -DWIN32_LEAN_AND_MEAN
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    )
endif()

# 编译器选项
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Linux (GCC/Clang) 通用选项
    add_compile_options(-Wall -Wextra -O2)
elseif(MSVC)
    set(CMAKE_GENERATOR_PLATFORM Win32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    add_compile_options("/utf-8" "/MP")
    add_link_options("/NODEFAULTLIB:LIBCMT")
    add_link_options("/NODEFAULTLIB:LIBCMTD")
endif()

# =========================================================
# 3. 自动扫描源文件
# =========================================================

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h" "include/*.hpp")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
file(GLOB_RECURSE BNCS_FILES CONFIGURE_DEPENDS "bncsutil/*.h" "bncsutil/*.c" "bncsutil/*.cpp")

add_executable(War3Bot ${SOURCES} ${HEADERS} ${BNCS_FILES})

# =========================================================
# 4. 资源复制 (war3files & config)
# =========================================================

# 1. 复制 war3files
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/war3files")
    add_custom_command(TARGET War3Bot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:War3Bot>/war3files"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/war3files" "$<TARGET_FILE_DIR:War3Bot>/war3files"
        COMMENT "正在复制 war3files..."
    )
else()
    message(WARNING "未找到 war3files 文件夹，请手动创建并放入 War3.exe, Storm.dll, Game.dll")
endif()

# 2. 复制 config
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
    add_custom_command(TARGET War3Bot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:War3Bot>/config"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/config" "$<TARGET_FILE_DIR:War3Bot>/config"
        COMMENT "正在复制 config..."
    )
endif()

target_compile_definitions(War3Bot PRIVATE APP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# =========================================================
# 5. 外部库配置 (GMP & StormLib)
# =========================================================

# 定义本地库根路径
set(LOCAL_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib")

# 包含公共头文件 (bncsutil等)
target_include_directories(War3Bot PRIVATE include bncsutil)

# --- Windows (MSVC) ---
if(MSVC)
    # [GMP]
    set(GMP_ROOT "${LOCAL_LIB_ROOT}/msvc/gmp")
    if(NOT EXISTS "${GMP_ROOT}/include/gmp.h")
        message(FATAL_ERROR "未找到 GMP 头文件: ${GMP_ROOT}/include/gmp.h")
    endif()

    target_include_directories(War3Bot PRIVATE "${GMP_ROOT}/include")
    target_link_libraries(War3Bot PRIVATE "${GMP_ROOT}/lib/gmp.lib")
    if(EXISTS "${GMP_ROOT}/lib/gmpxx.lib")
        target_link_libraries(War3Bot PRIVATE "${GMP_ROOT}/lib/gmpxx.lib")
    endif()

    # 部署 GMP DLL
    add_custom_command(TARGET War3Bot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GMP_ROOT}/bin/gmp-10.dll" "$<TARGET_FILE_DIR:War3Bot>/gmp-10.dll"
        COMMENT "部署 gmp-10.dll..."
    )
    if(EXISTS "${GMP_ROOT}/bin/gmpxx-4.dll")
        add_custom_command(TARGET War3Bot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GMP_ROOT}/bin/gmpxx-4.dll" "$<TARGET_FILE_DIR:War3Bot>/gmpxx-4.dll"
            COMMENT "部署 gmpxx-4.dll..."
        )
    endif()

    # [StormLib]
    set(STORMLIB_ROOT "${LOCAL_LIB_ROOT}/msvc/stormLib")
    target_include_directories(War3Bot PRIVATE "${STORMLIB_ROOT}/include")
    target_link_libraries(War3Bot PRIVATE
        "${STORMLIB_ROOT}/lib/StormLib.lib"
        "${STORMLIB_ROOT}/lib/zlib.lib"
        "${STORMLIB_ROOT}/lib/bz2.lib"
    )
    target_compile_definitions(War3Bot PRIVATE STORMLIB_NO_AUTO_LINK_LIB)

# --- Windows (MinGW) ---
elseif(MINGW)
    # [GMP]
    set(GMP_ROOT "${LOCAL_LIB_ROOT}/mingw/gmp")
    target_include_directories(War3Bot PRIVATE "${GMP_ROOT}/include")
    target_link_libraries(War3Bot PRIVATE "${GMP_ROOT}/lib/libgmpxx.a" "${GMP_ROOT}/lib/libgmp.a")

    # [StormLib]
    set(STORMLIB_ROOT "${LOCAL_LIB_ROOT}/mingw/stormLib")
    target_include_directories(War3Bot PRIVATE "${STORMLIB_ROOT}/include")
    target_link_libraries(War3Bot PRIVATE
        "${STORMLIB_ROOT}/lib/libStormLib.a"
        "${STORMLIB_ROOT}/lib/libzlib.a"
        "${STORMLIB_ROOT}/lib/libbz2.a"
    )

# --- Linux (UNIX) ---
elseif(UNIX)
    message(STATUS "Configuring for Linux...")

    set(LINUX_STORM_LIB "${LOCAL_LIB_ROOT}/linux/libstorm.a")

    if(EXISTS "${LINUX_STORM_LIB}")
        message(STATUS "Using local StormLib: ${LINUX_STORM_LIB}")
        find_package(ZLIB REQUIRED)
        find_library(BZIP2_LIB bz2)

        target_include_directories(War3Bot PRIVATE "${LOCAL_LIB_ROOT}/include")

        target_link_libraries(War3Bot PRIVATE
            "${LINUX_STORM_LIB}"
            ZLIB::ZLIB
            ${BZIP2_LIB}
            pthread
            dl
        )
    else()
        message(FATAL_ERROR "未在 ${LINUX_STORM_LIB} 找到库文件！请在 Linux 上编译 StormLib 并放入该目录。")
    endif()

    # [GMP] (通常使用系统库)
    find_library(GMP_LIB gmp)
    find_library(GMPXX_LIB gmpxx)
    if(GMP_LIB)
        target_link_libraries(War3Bot PRIVATE ${GMPXX_LIB} ${GMP_LIB})
    else()
        message(FATAL_ERROR "未找到 GMP 系统库，请运行: apt-get install libgmp-dev")
    endif()

endif()

# =========================================================
# 6. 公共链接配置
# =========================================================

# 基础 Qt 库
target_link_libraries(War3Bot PRIVATE Qt5::Core Qt5::Network)

# Windows 专用系统库
if(WIN32)
    target_link_libraries(War3Bot PRIVATE
        ws2_32
        version
        wininet
    )
endif()

# =========================================================
# 7. 安装配置
# =========================================================

# A. 安装可执行文件
install(TARGETS War3Bot RUNTIME DESTINATION bin)

# B. Linux 系统级资源安装
if(UNIX AND NOT APPLE)

    # 1. 安装配置目录 -> /etc/War3Bot/config
    install(DIRECTORY config
        DESTINATION "/etc/War3Bot"

        # 目录权限 (755): 必须有执行权限才能进入目录
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE

        # 文件权限 (644): 建议允许读取，否则普通用户运行程序可能读不到配置
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ

        # 可选：排除不需要的临时文件
        PATTERN "*~" EXCLUDE
        PATTERN "*.user" EXCLUDE
    )

    # 2. 安装 war3files 文件夹 -> /opt/War3Bot/war3files
    install(DIRECTORY war3files
        DESTINATION "war3files"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endif()
