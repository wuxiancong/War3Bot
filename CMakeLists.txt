cmake_minimum_required(VERSION 3.16)
project(War3Bot VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_MAKE_PROGRAM "D:/Qt/Tools/Ninja/ninja.exe" CACHE FILEPATH "Program used to build from makefiles" FORCE)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Network)

# 设置编译器选项
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
endif()

# 包含当前目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 设置自动 MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# =========================================================
# 自动扫描源文件和头文件
# =========================================================

# 1. 扫描 include 目录下的所有 .h 文件
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "include/*.h"
    "include/*.hpp"
)

# 2. 扫描 src 目录下的所有 .cpp 文件
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.c"
)

# 3. 扫描 bncsutil 目录下的文件 (因为你引入了这个库)
file(GLOB_RECURSE BNCS_FILES CONFIGURE_DEPENDS
    "bncsutil/*.h"
    "bncsutil/*.c"
    "bncsutil/*.cpp"
)

# =========================================================
# 创建可执行文件
# =========================================================

# 将扫描到的所有文件合并放入 add_executable
add_executable(War3Bot
    ${SOURCES}
    ${HEADERS}
    ${BNCS_FILES}
)

# 链接 Qt 库和网络库
target_link_libraries(War3Bot Qt5::Core Qt5::Network)

# 在 Windows 上链接 ws2_32 库
if(WIN32)
    target_link_libraries(War3Bot ws2_32)
endif()

# 设置包含目录
target_include_directories(War3Bot PRIVATE include)

# 安装目标
install(TARGETS War3Bot DESTINATION bin)

# 安装配置文件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/war3bot.ini")
    install(FILES config/war3bot.ini DESTINATION /etc/war3bot)
endif()

# 安装 systemd 服务文件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/war3bot.service")
    install(FILES config/war3bot.service DESTINATION /etc/systemd/system)
endif()
