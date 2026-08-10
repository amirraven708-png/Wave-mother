#ifndef RAVEN_VPN_H
#define RAVEN_VPN_H

#include <stdint.h>

typedef enum {
    VPN_DISCONNECTED,
    VPN_HANDSHAKING,
    VPN_CONNECTED,
    VPN_REKEYING
} vpn_state_t;

typedef struct {
    uint8_t  tx_key[32];
    uint8_t  rx_key[32];
    uint64_t tx_nonce;
    uint64_t rx_window;
    uint64_t rx_highest;
    uint32_t chain_counter;
    double   last_rekey;
    vpn_state_t state;
} raven_vpn_state_t;

void raven_vpn_init(raven_vpn_state_t *state, const uint8_t *root_key);
int  raven_vpn_chain_rekey(raven_vpn_state_t *state);
void raven_vpn_get_tx_nonce(raven_vpn_state_t *state, uint8_t *nonce_out);
int  raven_vpn_check_rx_nonce(raven_vpn_state_t *state, uint64_t nonce);

#endif
