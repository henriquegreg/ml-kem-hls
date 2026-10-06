#ifndef DEC_H
#define DEC_H

#include <stdint.h>
#include "params.h"

/*
 * DEC – Top-level HLS function for Kyber decapsulation
 *
 * Interface (Alveo U50):
 * ss     [out] – AXI4 master, write-only, KYBER_SSBYTES bytes
 * ct     [in]  – AXI4 master, read-only, KYBER_CIPHERTEXTBYTES bytes
 * sk     [in]  – AXI4 master, read-only, KYBER_SECRETKEYBYTES bytes
 * return       – AXI4-Lite slave (ap_start, ap_done, etc.)
 */
void dec(
    uint8_t ss_out[KYBER_SSBYTES],
    const uint8_t ct_in[KYBER_CIPHERTEXTBYTES],
    const uint8_t sk_in[KYBER_SECRETKEYBYTES]
);

#endif /* ENC_H */