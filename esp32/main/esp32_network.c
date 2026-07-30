/*
 * esp32_network.c - stub for the network subsystem (src/network/).
 *
 * Real NIC emulation needs libpcap/libslirp (host packet I/O) which
 * don't exist on ESP-IDF, and would need a full lwIP-backed rewrite -
 * out of scope for the M3 skeleton build. Every network_* entry point
 * called from outside src/network/ (86box.c, config.c) is stubbed
 * here as "no network devices available", so config parsing and
 * pc_reset()/pc_close() work but no card ever attaches.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <86box/86box.h>
#include <86box/device.h>
#include <86box/thread.h>
#include <86box/timer.h>
#include <86box/network.h>

/* config.c's load_network() calls inet_pton(AF_INET, ...) to turn a saved
 * "net_XX_addr" string into the base of a /24 for SLiRP - dead in
 * practice since no card ever attaches here, but still compiled
 * unconditionally. ESP-IDF's arpa/inet.h (lwip) doesn't declare it
 * without the full socket API, so provide the AF_INET dotted-quad case
 * directly instead of linking in lwip sockets for one string parse. */
int
inet_pton(int af, const char *src, void *dst)
{
    unsigned int b0, b1, b2, b3;
    uint8_t     *out = (uint8_t *) dst;

    (void) af;

    if (sscanf(src, "%u.%u.%u.%u", &b0, &b1, &b2, &b3) != 4)
        return 0;
    if ((b0 > 255) || (b1 > 255) || (b2 > 255) || (b3 > 255))
        return 0;

    out[0] = (uint8_t) b0;
    out[1] = (uint8_t) b1;
    out[2] = (uint8_t) b2;
    out[3] = (uint8_t) b3;
    return 1;
}

netcard_conf_t   ESP32_BIG_BSS_ATTR net_cards_conf[NET_CARD_MAX];
uint16_t         net_card_current = 0;
int              slirp_card_num   = 0;
network_devmap_t ESP32_BIG_BSS_ATTR network_devmap   = { 0 };
int              network_ndev     = 0;
netdev_t         ESP32_BIG_BSS_ATTR network_devs[NET_HOST_INTF_MAX];

void
network_init(void)
{
}

void
network_close(void)
{
}

void
network_reset(void)
{
}

int
network_available(void)
{
    return 0;
}

netcard_t *
network_attach(void *card_drv, uint8_t *mac, NETRXCB rx, NETSETLINKSTATE set_link_state)
{
    (void) card_drv;
    (void) mac;
    (void) rx;
    (void) set_link_state;
    return NULL;
}

void
netcard_close(netcard_t *card)
{
    (void) card;
}

void
network_tx(netcard_t *card, uint8_t *data, int len)
{
    (void) card;
    (void) data;
    (void) len;
}

void
network_connect(int id, int connect)
{
    (void) id;
    (void) connect;
}

int
network_is_connected(int id)
{
    (void) id;
    return 0;
}

int
network_dev_available(int id)
{
    (void) id;
    return 0;
}

int
network_dev_to_id(char *s)
{
    (void) s;
    return -1;
}

int
network_card_available(int id)
{
    (void) id;
    return 0;
}

int
network_card_has_config(int id)
{
    (void) id;
    return 0;
}

int
network_type_has_config(int id)
{
    (void) id;
    return 0;
}

const char *
network_card_get_internal_name(int id)
{
    (void) id;
    return "none";
}

int
network_card_get_from_internal_name(char *s)
{
    (void) s;
    return 0;
}

const device_t *
network_card_get_from_old_internal_name(char *s)
{
    (void) s;
    return NULL;
}

const device_t *
network_card_getdevice(int id)
{
    (void) id;
    return NULL;
}

int
network_tx_pop(netcard_t *card, netpkt_t *out_pkt)
{
    (void) card;
    (void) out_pkt;
    return 0;
}

int
network_tx_popv(netcard_t *card, netpkt_t *pkt_vec, int vec_size)
{
    (void) card;
    (void) pkt_vec;
    (void) vec_size;
    return 0;
}

int
network_rx_put(netcard_t *card, uint8_t *bufp, int len)
{
    (void) card;
    (void) bufp;
    (void) len;
    return 0;
}

int
network_rx_on_tx_popv(netcard_t *card, netpkt_t *pkt_vec, int vec_size)
{
    (void) card;
    (void) pkt_vec;
    (void) vec_size;
    return 0;
}

int
network_rx_on_tx_put(netcard_t *card, uint8_t *bufp, int len)
{
    (void) card;
    (void) bufp;
    (void) len;
    return 0;
}

int
network_rx_put_pkt(netcard_t *card, netpkt_t *pkt)
{
    (void) card;
    (void) pkt;
    return 0;
}

int
network_rx_on_tx_put_pkt(netcard_t *card, netpkt_t *pkt)
{
    (void) card;
    (void) pkt;
    return 0;
}
