#include "dmx.h"
#include "log.h"
#include "spark_atomic.h"

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define closesocket close
#endif

#define SPARK_DMX_ARTNET_PACKET_SIZE 530
#define SPARK_DMX_ARTPOLL_TIMEOUT_MS 500

typedef struct {
    int sock; // socket file descriptor (-1 when closed)
    struct sockaddr_in dest; // destination: IP + port 6454
} dmx_artnet_priv_t;

static dmx_artnet_priv_t s_dmx_artnet_priv;

static const uint8_t s_artpoll_packet[] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0', 0x00, 0x20, 0x00, 0x0e, 0x00, 0x00};

static int s_set_socket_timeout(int sock, int timeout_ms)
{
    #ifdef _WIN32
    DWORD t_ms = timeout_ms; 
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t_ms, sizeof(t_ms));
    #else
    struct timeval tv = { .tv_sec = timeout_ms / 1000, .tv_usec = timeout_ms * 1000 };
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    #endif
}

static int s_poll(spark_dmx_backend_t *backend)
{
    dmx_artnet_priv_t *priv = backend->priv;
    int written = sendto(priv->sock, (const char*)s_artpoll_packet, sizeof(s_artpoll_packet), 0, (struct sockaddr*)&priv->dest, sizeof(priv->dest));
    if (written < 0)
        return -1;

    uint8_t buffer[64];
    int read = recvfrom(priv->sock, (char*)buffer, sizeof(buffer), 0, (struct sockaddr*)&priv->dest, sizeof(priv->dest));
    if (read < 0)
        return -1;

    // TODO return memcmp();
}

static int s_open(spark_dmx_backend_t *backend)
{
    if (spark_atomic_load(&backend->state) == SPARK_DMX_CONNECTED)
        return 0;

    dmx_artnet_priv_t *priv = backend->priv;
    priv->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (priv->sock < 0)
    {
        spark_log_error("dmx_artnet: could not create socket");
        return -1;
    }
    s_set_socket_timeout(priv->sock, SPARK_DMX_ARTPOLL_TIMEOUT_MS);
    s_poll()

    spark_atomic_store(&backend->state, SPARK_DMX_CONNECTED);
    spark_atomic_inc(&backend->reconnects);
    return 0;
}

static void s_close(spark_dmx_backend_t *backend)
{
    dmx_artnet_priv_t *priv = backend->priv;
    closesocket(priv->sock);
    priv->sock = -1;
    spark_atomic_store(&backend->state, SPARK_DMX_DISCONNECTED);
}

static void s_build_packet(uint8_t *packet, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE])
{
    uint8_t header[] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0', 0x00, 0x50, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00};
    memcpy(packet, header, sizeof(header));
    memcpy(&packet[18], frame, SPARK_DMX_UNIVERSE_SIZE);
}

static int s_send_frame(spark_dmx_backend_t *backend, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE])
{
    if (spark_atomic_load(&backend->state) != SPARK_DMX_CONNECTED)
        return -1;

    uint8_t packet[SPARK_DMX_ARTNET_PACKET_SIZE];
    s_build_packet(packet, frame);

    /* Send packet here */
    dmx_artnet_priv_t *priv = backend->priv;
    int written = sendto(priv->sock, (const char*)packet, SPARK_DMX_ARTNET_PACKET_SIZE, 0, (struct sockaddr*)&priv->dest, sizeof(priv->dest));

    if (written < 0)
    {
        spark_atomic_inc(&backend->write_errors);
        spark_log_debug("dmx_artnet:send_frame: write failed, errors=%llu",
                        spark_atomic_load_u64(&backend->write_errors));
        return -1;
    }
    
    spark_atomic_inc(&backend->frames_sent);
    return 0;
}

static bool s_is_connected(spark_dmx_backend_t *backend)
{
    return spark_atomic_load(&backend->state) == SPARK_DMX_CONNECTED;
}

static const spark_dmx_ops_t s_artnet_ops = {
    .open = s_open,
    .close = s_close,
    .send_frame = s_send_frame,
    .is_connected = s_is_connected,
};

void spark_dmx_artnet_init(spark_dmx_backend_t *backend, const char *ip)
{
    backend->priv = &s_dmx_artnet_priv;
    spark_atomic_store(&backend->state, SPARK_DMX_DISCONNECTED);
    spark_dmx_reset_stats(backend);
    backend->ops = &s_artnet_ops;

    dmx_artnet_priv_t *priv = backend->priv;
    priv->sock = -1;
    inet_pton(AF_INET, ip, &priv->dest.sin_addr);
    priv->dest.sin_family = AF_INET;
    priv->dest.sin_port = htons(6454);
}