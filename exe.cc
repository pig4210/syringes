#include <xlib.h>

#pragma comment(lib, "xlib")

#include <iostream>
#include <conio.h>
#include <shlwapi.h>
#include <Tlhelp32.h>

#include "syringes.h"

static void console_out(const char* msg)
  {
  std::cout << msg << std::endl;
  }

static std::string getname(const char* filename)
  {
  int lpstart = 0;
  int lpend = 0;
  int i = 0;
  for(; filename[i] != '\0'; ++i)
    {
    switch(filename[i])
      {
      case '\\' : lpstart = i + 1; break;
      case '/' : lpstart = i + 1; break;
      case '.' : lpend = i; break;
      default: break;
      }
    }
  lpend = (lpend > lpstart) ? lpend : i;
  return std::string(&filename[lpstart], &filename[lpend]);
  }

DWORD getpid(const std::string& pname)
  {
  const auto name(s2ws(pname));
  auto hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(INVALID_HANDLE_VALUE == hSnapshot)
    {
    slog() << "CreateToolhelp32Snapshot fail : " << (uint32)GetLastError();
    return 0;
    }
  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(pe32);
  if(FALSE == Process32First(hSnapshot, &pe32))
    {
    slog() << "Process32First fail : " << (uint32)GetLastError();
    CloseHandle(hSnapshot);
    return 0;
    }
  do
    {
    if(0 == _wcsicmp(name.c_str(), pe32.szExeFile))
      return pe32.th32ProcessID;
    }while(Process32Next(hSnapshot, &pe32));
  CloseHandle(hSnapshot);
  return 0;
  }

bool UpperToken();
HANDLE LoadDll(HANDLE hProcess, const char* filename);
NTSTATUS UnloadDll(HANDLE hProcess, HANDLE hModule);

static std::string dllname;

// syringes 注入跳板时，会有阻塞，必须独立线程运行。
DWORD __stdcall spring(HANDLE hProcess)
  {
  slog() << __FUNCTION__ "  " << dllname << " in " << hProcess << " ...";
  LoadDll(hProcess, dllname.c_str());
  slog() << __FUNCTION__ " done.";
  return 0;
  }

static bool sendcmd(SOCKET sock, HANDLE hEvent, line& msg)
  {
  slog() << "wait dll socket...";
  const auto waitret = WaitForSingleObject(hEvent, gk_thread_wait);
  if(waitret == WAIT_TIMEOUT)
    {
    slog() << "WaitForSingleObject timeout.";
    return false;
    }
  if(waitret == WAIT_FAILED || waitret != WAIT_OBJECT_0)
    {
    slog() << "WaitForSingleObject fail : " << (uint32)waitret << " : " << (uint32)GetLastError();
    return false;
    }
  auto addr = AddrInfo(INADDR_LOOPBACK, gk_bindport);
  slog() << "\r\nsendto : " << IpString(addr) << " :\r\n"<< showbin(msg);
  char tmp[0x10];
  if(SOCKET_ERROR == sendto(sock, (const char*)msg.c_str(), (int)msg.size(), 0, (const sockaddr*)&addr, sizeof(addr)))
    {
    slog() << "sendto fail : " << (int32)WSAGetLastError();
    return false;
    }
  SetEvent(hEvent);

  slog() << "waiting ack...";
  int addrlen = sizeof(addr);
  int recvlen = recvfrom(sock, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&addr, &addrlen);
  if(recvlen == 0)
    {
    slog() << "socket closed.";
    return false;
    }
  if(recvlen == SOCKET_ERROR)
    {
    slog() << "sock recv fail : " << (int32)WSAGetLastError();
    return false;
    }

  slog() << "waiting result...";
  addrlen = sizeof(addr);
  recvlen = recvfrom(sock, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&addr, &addrlen);
  if(recvlen == 0)
    {
    slog() << "socket closed.";
    return false;
    }
  if(recvlen == SOCKET_ERROR)
    {
    slog() << "sock recv fail : " << (int32)WSAGetLastError();
    return false;
    }
  slog() << "\r\n" << showbin(tmp, recvlen);
  msg.assign((const uint8*)tmp, recvlen);
  return true;
  }

int main(int argc, const char* argv[])
  {
  slog::out = console_out;

  const auto myname(getname(argv[0]));
  dllname = myname + ".dll";

  if(argc <= 1 || argc > 4)
    {
    slog() << "\r\nusage : \r\n\r\n    " << myname << " [ PID|ProcessName PID|ProcessName ] DllFileName|DllModule";
    return 0;
    }

  UpperToken();   // 无论是否提权成功，都尝试继续。

  // 最后一个参数视为 DllFileName|DllModule 。决定 load or unload 。
  const auto dstdllname = std::string(argv[argc - 1]);
  size_t readlen = 0;
  const auto hModule = hex2value(dstdllname, &readlen, 0, true, true);
  const bool load = (readlen != dstdllname.size());
  if(load)
    {
    slog() << "load [ " << dstdllname << " ].";
    }
  else
    {
    slog() << "unlod [ " << (void*)hModule << " ].";
    }
  
  // 尝试解析多余参数为 PID 。
  DWORD pids[2];
  for(int i = 1; i < argc - 1; ++i)
    {
    const auto pn = std::string(argv[i]);
    size_t rl = 0;
    DWORD pid = (DWORD)hex2value(pn, &rl, 0, true, true);
    if(rl != pn.size() || rl > (sizeof(DWORD) * sizeof(HEX_VALUE_STRUCT)))
      {
      pid = getpid(pn);
      }
    if(pid == 0)
      {
      slog() << "Can't find [ " << pn << " ].";
      _getch();
      return 1;
      }
    slog() << "    " << (uint32)pid << " << " << pn;
    pids[i - 1] = pid;
    }

  HANDLE hProcess;
  if(argc == 2)
    {
    hProcess = GetCurrentProcess();
    }
  else
    {
    const auto pid = pids[0];
    hProcess = OpenProcess(PROCESS_CREATE_THREAD|PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE, TRUE, pid);
    if(hProcess == nullptr)
      {
      slog() << "OpenProcess fail : " << (uint32)GetLastError();
      _getch();
      return 1;
      }
    slog() << "OpenProcess " << (uint32)pid << " : " << hProcess;
    }

  // 无 pid 操作本进程。 一个 pid 则无需跳板。
  if(argc != 4)
    {
    if(load)
      {
      auto h = LoadDll(hProcess, dstdllname.c_str());
      CloseHandle(hProcess);
      _getch();
      return h == nullptr ? 1 : 0;
      }
    const auto n = UnloadDll(hProcess, (HANDLE)hModule);
    CloseHandle(hProcess);
    _getch();
    return NT_SUCCESS(n) ? 0 : 1;
    }

  // 跳板操作
  auto hEvent = CreateEvent(nullptr, FALSE, FALSE, gk_event_name);
  if(nullptr == hEvent)
    {
    slog() << "CreateEvent fail : " << (uint32)GetLastError();
    CloseHandle(hProcess);
    _getch();
    return 1;
    }
  
  xWSA wsa;
  SOCKET sock = socket(PF_INET, SOCK_DGRAM, 0);
  if(sock == INVALID_SOCKET)
    {
    slog() << "socket fail : " << (int32)WSAGetLastError();
    CloseHandle(hEvent);
    CloseHandle(hProcess);
    _getch();
    return 1;
    }
  const unsigned int timeout = gk_sock_timeout + 1000;
  if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)))
    {
    slog() << "sock set timeout fail : " << (int32)WSAGetLastError();
    shutdown(sock, SD_BOTH);
    closesocket(sock);
    CloseHandle(hEvent);
    CloseHandle(hProcess);
    _getch();
    return 1;
    }

  line data;
  data << (uint32)pids[1];
  if(load)
    {
    data << dstdllname;
    }
  else
    {
    data << (void*)hModule;
    }

  auto hThread = CreateThread(nullptr, 0, &spring, hProcess, 0, nullptr);
  if(hThread == nullptr)
    {
    slog() << "CreateThread fail : " << (uint32)GetLastError();
    shutdown(sock, SD_BOTH);
    closesocket(sock);
    CloseHandle(hEvent);
    CloseHandle(hProcess);
    _getch();
    return 1;
    }
  // 注意线程一创建，Event 需要立即进入等待，以免被跳板抢先。
  const auto b = sendcmd(sock, hEvent, data);
  CloseHandle(hEvent);
    
  shutdown(sock, SD_BOTH);
  closesocket(sock);
  WaitForSingleObject(hThread, gk_thread_wait + 2);
  CloseHandle(hThread);
  CloseHandle(hProcess);
  if(b)
    {
    if(load)
      slog() << myname << " done : " << *(void**)data.c_str();
    else
      slog() << myname << " done : " << *(uint32*)data.c_str();
    }
  else
    {
    slog() << myname << " fail.";
    }
  _getch();
  return 0;
  }