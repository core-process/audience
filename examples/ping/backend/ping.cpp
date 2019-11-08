#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <cstdint>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#endif

#include "ping.h"

#if defined(__APPLE__) || defined(_WIN32)
#define SOL_IP IPPROTO_IP
#endif

#if !defined(_WIN32)
#define closesocket close
#endif

#ifdef _WIN32
#define ICMP_ECHO 8
#endif

using namespace std::chrono_literals;

struct echo_package
{
#ifdef _WIN32
  typedef struct icmphdr
  {
    uint8_t type; /* message type */
    uint8_t code; /* type sub-code */
    uint16_t checksum;
    union {
      struct
      {
        uint16_t id;
        uint16_t sequence;
      } echo;           /* echo datagram */
      uint32_t gateway; /* gateway address */
      struct
      {
        uint16_t __unused;
        uint16_t mtu;
      } frag; /* path mtu discovery */
    } un;
  } icmp_header;
#elif __APPLE__
  typedef struct icmp icmp_header;
#elif __linux__
  typedef struct icmphdr icmp_header;
#else
#error "Unsupported target"
#endif
  icmp_header hdr;
  char msg[64 - sizeof(icmp_header)];
};

static uint16_t ping_package_checksum(echo_package &pkg);

static std::unique_ptr<std::thread> ping_thread;
static std::atomic<bool> ping_stop_signal = false;

void ping_start(
    std::function<void(ping_time_point, ping_duration)> on_echo_reply,
    std::function<void(std::string)> on_error)
{
#ifdef _WIN32
  WSADATA wsock;
  if (WSAStartup(MAKEWORD(2, 2), &wsock) != 0)
  {
    on_error("could not initialize windows sockets");
    return;
  }
#endif

  ping_thread = std::make_unique<std::thread>([on_echo_reply, on_error]() {
    // create datagram socket
#ifdef __APPLE__
    auto sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
#else
    auto sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
#endif
    if (sock < 0)
    {
      on_error("could not open datagram socket");
      return;
    }

    // set ttl
    int ttl = 64;
    if (setsockopt(sock, SOL_IP, IP_TTL, (const char *)&ttl, sizeof(ttl)) != 0)
    {
      on_error("could not set ttl");
      closesocket(sock);
      return;
    }

    // set receive timeout
    timeval rcv_timeout;
    rcv_timeout.tv_sec = 5;
    rcv_timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&rcv_timeout, sizeof rcv_timeout) != 0)
    {
      on_error("could not set receive timeout");
      closesocket(sock);
      return;
    }

    // our ping target address: 8.8.8.8
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0x08080808;

    // ping loop
    for (uint16_t seq = 0; !ping_stop_signal; ++seq)
    {
      // prepare echo package
      echo_package pkg{};
#ifdef __APPLE__
      pkg.hdr.icmp_type = ICMP_ECHO;
      pkg.hdr.icmp_id = getpid();
      pkg.hdr.icmp_seq = seq;
#else
      pkg.hdr.type = ICMP_ECHO;
      pkg.hdr.un.echo.id = getpid();
      pkg.hdr.un.echo.sequence = seq;
#endif
      for (size_t i = 0; i < sizeof(pkg.msg) - 1; i++)
      {
        pkg.msg[i] = i + '0';
      }
#ifdef __APPLE__
      pkg.hdr.icmp_cksum = ping_package_checksum(pkg);
#else
      pkg.hdr.checksum = ping_package_checksum(pkg);
#endif

      // send echo package
      ping_time_point tp1 = std::chrono::system_clock::now();

      if (sendto(sock, (const char *)&pkg, sizeof(pkg), 0, (struct sockaddr *)&addr, sizeof(addr)) <= 0)
      {
        on_error("could not send echo package");
        std::this_thread::sleep_for(3s);
        continue;
      }

      // receive echo reply package
      sockaddr_in rcv_addr;
      socklen_t rcv_addr_len = sizeof(rcv_addr);
      if (recvfrom(sock, (char *)&pkg, sizeof(pkg), 0, (struct sockaddr *)&rcv_addr, &rcv_addr_len) <= 0)
      {
        on_error("could not receive echo reply package");
        std::this_thread::sleep_for(3s);
        continue;
      }

      // evaluate reply package
#ifdef __APPLE__
      if (pkg.hdr.icmp_type == 69 && pkg.hdr.icmp_code == 0)
#else
      if (pkg.hdr.type == 69 && pkg.hdr.code == 0)
#endif
      {
        ping_time_point tp2 = std::chrono::system_clock::now();
        ping_duration dur = tp2 - tp1;

        if (dur.count() >= 0)
        {
          on_echo_reply(tp1, dur);
        }
        std::this_thread::sleep_for(1s);
      }
      else
      {
        on_error("invalid echo reply package received");
        std::this_thread::sleep_for(3s);
      }
    }

    // close socket properly
    closesocket(sock);
  });
}

void ping_stop()
{
  ping_stop_signal = true;

  if (ping_thread)
  {
    ping_thread->join();
    ping_thread.reset();
  }
}

static uint16_t ping_package_checksum(echo_package &pkg)
{
  size_t len = sizeof(echo_package);
  uint16_t *buf = (uint16_t *)&pkg;
  uint32_t sum = 0;
  uint16_t result;
  for (sum = 0; len >= 2; len -= 2)
    sum += *buf++;
  if (len == 1)
    sum += *(uint8_t *)buf;
  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  result = ~sum;
  return result;
}
