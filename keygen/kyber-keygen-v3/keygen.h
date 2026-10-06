#ifndef KEYGEN_H
#define KEYGEN_H

#include <stdint.h>
#include "params.h"

/*
 * keygen – Top-level HLS function for Kyber key generation
 *
 * Interface (Alveo U50):
 *   coins [in]  – AXI4 master, read-only, 2*KYBER_SYMBYTES bytes
 *                 Pre-filled by the host with cryptographically
 *                 secure random data before issuing ap_start.
 *   pk    [out] – AXI4 master, write-only, KYBER_PUBLICKEYBYTES bytes
 *   sk    [out] – AXI4 master, write-only, KYBER_SECRETKEYBYTES bytes
 *   return      – AXI4-Lite slave (ap_start, ap_done,
 *                 ap_idle, ap_ready + scalar port addresses)
 */
void keygen(const uint8_t coins[2 * KYBER_SYMBYTES],
                uint8_t       pk[KYBER_PUBLICKEYBYTES],
                uint8_t       sk[KYBER_SECRETKEYBYTES]);

#endif /* KEYGEN_H */