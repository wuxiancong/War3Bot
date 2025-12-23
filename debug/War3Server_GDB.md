# War3Server 崩溃调试完整指南

## 第一步：清理并重新编译 (Debug 模式)

这是最关键的一步。我们必须清除旧的编译文件，并开启 `Debug` 模式以生成代码行号信息。

```bash
# 1. 停止正在运行的服务
sudo killall -9 bnetd gdb

# 2. 进入项目目录
cd War3Server

# 3. 删除旧的构建目录 (确保干净构建)
rm -rf build
mkdir build && cd build

# 4. 配置 CMake (注意：这里改成了 Debug 模式！)
cmake .. \
  -D WITH_LUA=true \
  -D WITH_MYSQL=true \
  -D MYSQL_INCLUDE_DIR=/usr/include/mysql \
  -D CMAKE_INSTALL_PREFIX=/usr/local/War3Server \
  -D MYSQL_LIBRARY=/usr/lib/x86_64-linux-gnu/libmysqlclient.so \
  -D CMAKE_BUILD_TYPE=Debug

# 5. 编译与安装
make -j$(nproc)
sudo make install
```

> **注意**：`CMAKE_BUILD_TYPE=Debug` 是让 GDB 能显示代码行号的关键。

---

## 第二步：启动 GDB 调试

现在二进制文件已经包含了调试符号，我们开始抓取崩溃现场。

```bash
# 1. 启动 GDB (指向安装好的可执行文件)
sudo gdb /usr/local/War3Server/sbin/bnetd
```

进入 GDB 界面（看到 `(gdb)` 提示符）后，**依次输入**以下命令：

```gdb
# 【重要】强制前台运行，否则 GDB 抓不到子进程
(gdb) set args -f

# 2. 运行服务器
(gdb) run
```

此时服务器应该正常启动，终端会停留在运行状态（不会退回命令行）。

---

## 第三步：触发崩溃并获取堆栈

1.  保持 GDB 窗口开启，不要关闭。
2.  打开《魔兽争霸3》客户端。
3.  进入局域网/战网，**刷新游戏列表**，或者执行导致崩溃的操作。
4.  等待 GDB 提示崩溃信息（通常是 `Program received signal SIGSEGV`）。

---

## 第四步：导出错误堆栈 (Backtrace)

当崩溃发生、GDB 暂停时，输入以下命令：

```gdb
(gdb) bt
```

**请将 `bt` 命令输出的所有内容复制发出来，这将直接定位到代码崩溃的具体行号。**

---

### (可选) 调试完成后恢复高性能版本

修复 Bug 后，记得切回 Release 模式以获得最佳性能：

```bash
cd War3Server/build
rm -rf *
cmake .. -D ... -D CMAKE_BUILD_TYPE=Release  # 其他参数保持不变
make -j$(nproc)
sudo make install
```