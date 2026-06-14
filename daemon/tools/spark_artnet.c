#include "consts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
static uint64_t s_now_us(void)
{
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000 / freq.QuadPart);
}
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#define closesocket close
static uint64_t s_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

#define ARTNET_PORT 6454
#define DEFAULT_COUNT 5
#define DEFAULT_TIMEOUT_MS 1000
#define DEFAULT_INTERVAL_MS 1000

static const uint8_t s_artpoll_packet[] = {
    'A', 'r', 't', '-', 'N', 'e', 't', '\0',
    0x00, 0x20,
    0x00, 0x0e,
    0x00, 0x00
};

static const uint8_t s_artpoll_reply_head[] = {
    'A', 'r', 't', '-', 'N', 'e', 't', '\0',
    0x00, 0x21
};

static volatile int s_interrupted = 0;

#ifndef _WIN32
static void s_sigint_handler(int sig) { (void)sig; s_interrupted = 1; }
#endif

static void usage(void)
{
    fprintf(stderr, "spark-artnet - Art-Net node ping tool\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  spark-artnet poll <ip> [-c count] [-i interval_ms] [-t timeout_ms]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Sends ArtPoll packets to the given IP on port 6454 (UDP)\n");
    fprintf(stderr, "and measures round-trip time for each ArtPollReply.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -c count          Number of polls to send (default: %d)\n", DEFAULT_COUNT);
    fprintf(stderr, "  -i interval_ms    Interval between polls in ms (default: %d)\n", DEFAULT_INTERVAL_MS);
    fprintf(stderr, "  -t timeout_ms     Timeout per poll in ms (default: %d)\n", DEFAULT_TIMEOUT_MS);
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  spark-artnet poll 192.168.1.100\n");
    fprintf(stderr, "  spark-artnet poll 192.168.1.100 -c 10 -i 500\n");
}

static void s_sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

static int cmd_poll(const char *ip, int count, int interval_ms, int timeout_ms)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        fprintf(stderr, "Error: WSAStartup failed\n");
        return 1;
    }
#else
    signal(SIGINT, s_sigint_handler);
#endif

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        fprintf(stderr, "Error: could not create socket\n");
        return 1;
    }

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast));

#ifdef _WIN32
    DWORD tv = timeout_ms;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#else
    struct timeval tv = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(ARTNET_PORT);
    if (inet_pton(AF_INET, ip, &dest.sin_addr) != 1)
    {
        fprintf(stderr, "Error: invalid IP address '%s'\n", ip);
        closesocket(sock);
        return 1;
    }

    printf("ARTPOLL %s port %d: %d polls, timeout %d ms, interval %d ms\n\n",
           ip, ARTNET_PORT, count, timeout_ms, interval_ms);

    int sent = 0;
    int received = 0;
    double rtt_min = 1e9;
    double rtt_max = 0;
    double rtt_sum = 0;
    char node_name[19] = {0};

    for (int seq = 0; seq < count && !s_interrupted; seq++)
    {
        uint64_t t_send = s_now_us();

        int written = sendto(sock, (const char *)s_artpoll_packet, sizeof(s_artpoll_packet), 0,
                             (struct sockaddr *)&dest, sizeof(dest));
        if (written < 0)
        {
            printf("seq %d: send failed\n", seq + 1);
            sent++;
            if (seq < count - 1 && !s_interrupted)
                s_sleep_ms(interval_ms);
            continue;
        }
        sent++;

        uint8_t buf[512];
        struct sockaddr_in src = {0};
        socklen_t srclen = sizeof(src);
        int n = recvfrom(sock, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&src, &srclen);

        if (n < 0)
        {
            printf("seq %d: timeout\n", seq + 1);
        }
        else if (n >= (int)sizeof(s_artpoll_reply_head) &&
                 memcmp(buf, s_artpoll_reply_head, sizeof(s_artpoll_reply_head)) == 0)
        {
            uint64_t t_recv = s_now_us();
            double rtt_ms = (t_recv - t_send) / 1000.0;

            received++;
            rtt_sum += rtt_ms;
            if (rtt_ms < rtt_min) rtt_min = rtt_ms;
            if (rtt_ms > rtt_max) rtt_max = rtt_ms;

            char src_ip[64];
            inet_ntop(AF_INET, &src.sin_addr, src_ip, sizeof(src_ip));

            if (!node_name[0] && n >= 44)
                memcpy(node_name, &buf[26], 18);

            printf("seq %d: reply from %s  time=%.2f ms\n", seq + 1, src_ip, rtt_ms);
        }
        else
        {
            printf("seq %d: unexpected response (%d bytes)\n", seq + 1, n);
        }

        if (seq < count - 1 && !s_interrupted)
            s_sleep_ms(interval_ms);
    }

    closesocket(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    /* Stats */
    printf("\n--- %s ArtPoll statistics ---\n", ip);
    if (node_name[0])
        printf("Node: %s\n", node_name);
    int lost = sent - received;
    double loss_pct = sent > 0 ? (lost * 100.0 / sent) : 0;
    printf("%d packets transmitted, %d received, %.0f%% packet loss\n", sent, received, loss_pct);
    if (received > 0)
    {
        double rtt_avg = rtt_sum / received;
        printf("rtt min/avg/max = %.2f/%.2f/%.2f ms\n", rtt_min, rtt_avg, rtt_max);
    }

    return received > 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--version") == 0)
    {
        printf("spark-artnet, from sparkd v%s\n", SPARKD_VERSION);
        return 0;
    }
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
    {
        usage();
        return argc < 2 ? 1 : 0;
    }

    if (strcmp(argv[1], "poll") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Error: 'poll' requires an IP address\n\n");
            usage();
            return 1;
        }

        const char *ip = argv[2];
        int count = DEFAULT_COUNT;
        int interval_ms = DEFAULT_INTERVAL_MS;
        int timeout_ms = DEFAULT_TIMEOUT_MS;

        for (int i = 3; i < argc - 1; i += 2)
        {
            if (strcmp(argv[i], "-c") == 0)
                count = atoi(argv[i + 1]);
            else if (strcmp(argv[i], "-i") == 0)
                interval_ms = atoi(argv[i + 1]);
            else if (strcmp(argv[i], "-t") == 0)
                timeout_ms = atoi(argv[i + 1]);
        }

        if (count < 1) count = 1;
        if (timeout_ms < 100) timeout_ms = 100;
        if (interval_ms < 100) interval_ms = 100;

        return cmd_poll(ip, count, interval_ms, timeout_ms);
    }
    else
    {
        fprintf(stderr, "spark-artnet: unknown command '%s'\n", argv[1]);
        usage();
        return 1;
    }
}
