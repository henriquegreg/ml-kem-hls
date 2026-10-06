#ifndef RANDOMBYTES_H
#define RANDOMBYTES_H

#include <stddef.h>
#include <stdint.h>

/*
 * HLS note: randombytes is NOT called from inside the accelerator.
 * The top-level function (keygen_top) receives the 2*KYBER_SYMBYTES
 * coins buffer as an AXI master port pre-filled by the host.
 * This header is kept to allow kem.c to compile unchanged in both
 * software simulation and HLS contexts.
 */
void randombytes(uint8_t *out, size_t outlen);

#endif