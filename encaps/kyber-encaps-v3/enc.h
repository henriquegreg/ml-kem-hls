#ifndef ENC_H
#define ENC_H

#include <stdint.h>
#include "params.h"

/*
 * enc – Top-level HLS function for Kyber encapsulation
 *
 * Interface (Alveo U50):
 * pk    [in]  – AXI4 master, read-only, KYBER_PUBLICKEYBYTES bytes
 * coins [in]  – AXI4 master, read-only, KYBER_SYMBYTES bytes (aleatoriedade)
 * ct    [out] – AXI4 master, write-only, KYBER_CIPHERTEXTBYTES bytes
 * ss    [out] – AXI4 master, write-only, KYBER_SSBYTES bytes
 * return      – AXI4-Lite slave (ap_start, ap_done, etc.)
 */
void enc(const uint8_t pk[KYBER_PUBLICKEYBYTES],
         const uint8_t coins[KYBER_SYMBYTES],
         uint8_t       ct[KYBER_CIPHERTEXTBYTES],
         uint8_t       ss[KYBER_SSBYTES]);

#endif /* ENC_H */