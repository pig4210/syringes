#ifndef _SYRINGES_CONFIG_H_
#define _SYRINGES_CONFIG_H_

const unsigned short gk_bindport = 0x59E5;          ///< 绑定 UDP 端口号。
const int gk_sock_timeout = 5000;                   ///< UDP recv 超时时间。 5s 。
const unsigned int gk_thread_wait = 10000;          ///< 远程线程的等待时间。
const auto gk_event_name = TEXT("Global_syringes"); ///< Event 对象，用于跳板操作时。

class slog : public xmsg
  {
  public:
    virtual ~slog();
  public:
    static void (*out)(const char*);
  };
#endif