# syringes

syringes 分为独立的两个部分：

- [syringes.exe](#syringes.exe) 用于 控制台 操作。
- [syringes.dll](#syringes.dll) 用于 跳板 ，或 第三方操纵。

------- ------- ------- ------- 

## syringes.exe

注入 DLL 使用命令行如下：

- 在 本进程 测试注入： `syringes DllName` 。
- 指定 进程名 注入： `syringes ProcessName DllName` 。
- 指定 进程 ID 注入： `syringes PID DllName` 。
- 指定 跳板进程 与 目标进程 注入：
    - `syringes SpringProcessName DestProcessName DllName` 。
    - `syringes SpringPID DestPID DllName` 。
    - `syringes SpringProcessName DestPID DllName` 。
    - `syringes SpringPID DestProcessName DllName` 。

卸载 DLL 使用命令行如下：

- 在 本进程 测试卸载 DLL ： `syringes DllModule` 。（一般不会使用。）
- 指定 进程名 卸载： `syringes ProcessName DllModule` 。
- 指定 进程 ID 卸载： `syringes PID DllModule` 。
- 指定 跳板进程 与 目标进程 卸载：
    - `syringes SpringProcessName DestProcessName DllModule` 。
    - `syringes SpringPID DestPID DllModule` 。
    - `syringes SpringProcessName DestPID DllModule` 。
    - `syringes SpringPID DestProcessName DllModule` 。

注意事项：

- 进程名请写完整，如 `test.exe` ，而不是 `test` 。
- 如果存在多个同名进程，请使用 `PID` 精确指定。

------- ------- ------- ------- 

## syringes.dll

DLL 返回 FALSE ，动作完成后不驻留，无痕迹。

考虑第三方使用的便利，不使用管道通讯。

第三方调用方法：

1. 加载 syringes.dll 。（ syringes.dll 加载时阻塞，直至操作完成。所以以下操作需要另外线程并发。）
1. `udp://localhost:23013` 发送如下数据结构：
    - 加载：
        ```C++
        struct
            {
            DWORD PID;          // 需要加载进程的 PID 。
            char dllname[];     // 需要加载的 DllName 。
            };
        // 88 88 00 00 74 65 73 74    # PID == 0x00008888    DllName == "test"
        ```
    - 卸载：
        ```C++
        struct
            {
            DWORD PID;          // 需要卸载进程的 PID 。
            HMODULE hMoudle;    // 需要卸载的 Dll Module handle 。
            };
        // 88 88 00 00 00 00 00 10              # x86 PID == 0x00008888    DllModule == 0x10000000
        // 88 88 00 00 00 00 00 00 00 00 00 10  # x64 PID == 0x00008888    DllModule == 0x1000000000000000
        ```
1. 返回 数据 ： "ok" 表示成功接收到数据，等待操作完成。
1. 返回 操作结果。
    - 加载：返回 nullptr 表示失败，或 加载成功的 DllModule 。
    - 卸载：返回 0 表示成功，或 NTSTATUS 。
    
------- ------- ------- ------- 

## 可能的错误码

```
00000005 : 拒绝访问。       x86 不能访问 x64 进程。
00000006 : 句柄无效。
0000012B : 仅完成部分的 ReadProcessMemory 或 WriteProcessMemory 请求。 x64 访问 x86 出错。
C0000135 : STATUS_DLL_NOT_FOUND    Dll 文件未找到。
C0000145 : STATUS_APP_INIT_FAILURE    Dll 初始化失败。
```