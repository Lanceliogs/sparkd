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
#include <fcntl.h>
#define closesocket close
#endif

#define SPARK_DMX_ARTNET_PACKET_SIZE 530

#define SPARK_DMX_ARTPOLL_POLL_EVERY 75
#define SPARK_DMX_ARTPOLL_CHECK_EVERY 5

typedef struct {
    int sock;
    struct sockaddr_in dest;
    int poll_check_tries;
} dmx_artnet_priv_t;

static dmx_artnet_priv_t s_dmx_artnet_priv;

static const uint8_t s_artpoll_packet[] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0', 0x00, 0x20, 0x00, 0x0e, 0x00, 0x00};
static const uint8_t s_artpoll_response_head[] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0', 0x00, 0x21};



static int s_set_socket_nonblocking(int sock)
{
    #ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
    #else
    return fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    #endif
}

static int s_poll(spark_dmx_backend_t *backend)
{
    dmx_artnet_priv_t *priv = backend->priv;
    int written = sendto(priv->sock, (const char*)s_artpoll_packet, sizeof(s_artpoll_packet), 0, (struct sockaddr*)&priv->dest, sizeof(priv->dest));
    if (written < 0)
        return -1;
    
    priv->poll_check_tries = 3;
    return 0;
}

static int s_check_poll_response(spark_dmx_backend_t *backend)
{
    dmx_artnet_priv_t *priv = backend->priv;
    priv->poll_check_tries--;

    uint8_t buffer[256];
    int read = recvfrom(priv->sock, (char*)buffer, sizeof(buffer), 0, NULL, NULL);
    if (read < 0)
    {
        if (priv->poll_check_tries == 0)
        {
            spark_atomic_store(&backend->node_responsive, false);
            spark_log_debug("artnet:poll: no response");
        }
        return -1;
    }

    if (memcmp(buffer, s_artpoll_response_head, sizeof(s_artpoll_response_head)) == 0)
    {
        spark_atomic_store(&backend->node_responsive, true);
        priv->poll_check_tries = 0;

        char short_name[19] = {0};
        if (read >= 44)
            memcpy(short_name, &buffer[26], 18);
        spark_log_info("artnet:poll: node alive (%s)", short_name[0] ? short_name : "unnamed");
        return 0;
    }

    spark_log_warn("artnet:poll: unexpected response received to artpoll packet");
    return -1;
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
    s_set_socket_nonblocking(priv->sock);

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

    dmx_artnet_priv_t *priv = backend->priv;
    uint64_t frames_sent = spark_atomic_load(&backend->frames_sent);
    if (frames_sent % SPARK_DMX_ARTPOLL_POLL_EVERY == 0)
        s_poll(backend);
    if (priv->poll_check_tries > 0 && frames_sent % SPARK_DMX_ARTPOLL_CHECK_EVERY == 0)
        s_check_poll_response(backend);

    uint8_t packet[SPARK_DMX_ARTNET_PACKET_SIZE];
    s_build_packet(packet, frame);

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