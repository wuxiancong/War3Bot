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

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -O2)
elseif(MSVC)
    add_compile_options(/W3 /O2)
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
# ★★★【新增】自动复制 war3files 和 config 到生成目录 ★★★
# =========================================================

# 1. 复制 war3files 文件夹 (如果源码目录下存在)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/war3files")
    add_custom_command(TARGET War3Bot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:War3Bot>/war3files
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
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:War3Bot>/config
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/config"
            "$<TARGET_FILE_DIR:War3Bot>/config"
        COMMENT "正在将 config 目录复制到构建目录..."
    )
endif()

# =========================================================
# 4. GMP 库配置
# =========================================================

set(LOCAL_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib")

if(MINGW)
    set(GMP_INCLUDE_DIR "${LOCAL_LIB_ROOT}/mingw/gmp/include")
    set(GMP_LIB_DIR "${LOCAL_LIB_ROOT}/mingw/gmp/lib")
    target_include_directories(War3Bot PRIVATE "${GMP_INCLUDE_DIR}")
    target_link_libraries(War3Bot PRIVATE
        "${GMP_LIB_DIR}/libgmpxx.a"
        "${GMP_LIB_DIR}/libgmp.a"
    )
elseif(MSVC)
    set(GMP_INCLUDE_DIR "${LOCAL_LIB_ROOT}/msvc/gmp/include")
    set(GMP_LIB_DIR "${LOCAL_LIB_ROOT}/msvc/gmp/lib")
    target_include_directories(War3Bot PRIVATE "${GMP_INCLUDE_DIR}")
    target_link_libraries(War3Bot PRIVATE "${GMP_LIB_DIR}/gmp.lib")
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GMP gmp gmpxx)
    endif()
    if(GMP_FOUND)
        target_include_directories(War3Bot PRIVATE ${GMP_INCLUDE_DIRS})
        target_link_libraries(War3Bot PRIVATE ${GMP_LIBRARIES})
    else()
        target_link_libraries(War3Bot PRIVATE gmpxx gmp)
    endif()
endif()

# =========================================================
# 5. 公共链接配置
# =========================================================

target_include_directories(War3Bot PRIVATE include bncsutil) # 确保能include bncsutil
target_link_libraries(War3Bot PRIVATE Qt5::Core Qt5::Network)

if(WIN32)
    target_link_libraries(War3Bot PRIVATE ws2_32)
endif()

# =========================================================
# 6. 安装配置
# =========================================================

install(TARGETS War3Bot DESTINATION bin)
