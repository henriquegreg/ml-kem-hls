#include <string.h>
#include <stdint.h>
#include "params.h"
#include "enc.h"
#include "kem.h"

void enc(const uint8_t pk[KYBER_PUBLICKEYBYTES],
         const uint8_t coins[KYBER_SYMBYTES],
         uint8_t       ct[KYBER_CIPHERTEXTBYTES],
         uint8_t       ss[KYBER_SSBYTES])
{
  /*
   * ── Interface pragmas ────────────────────────────────────────────────
   */
#pragma HLS INTERFACE m_axi port=pk     offset=slave bundle=gmem_pk    \
                            depth=KYBER_PUBLICKEYBYTES \
                            max_read_burst_length=256 \
                            max_widen_bitwidth=512

#pragma HLS INTERFACE m_axi port=coins  offset=slave bundle=gmem_coins \
                            depth=KYBER_SYMBYTES \
                            max_read_burst_length=256 \
                            max_widen_bitwidth=512

#pragma HLS INTERFACE m_axi port=ct     offset=slave bundle=gmem_ct    \
                            depth=KYBER_CIPHERTEXTBYTES \
                            max_write_burst_length=256 \
                            max_widen_bitwidth=512

#pragma HLS INTERFACE m_axi port=ss     offset=slave bundle=gmem_ss    \
                            depth=KYBER_SSBYTES \
                            max_write_burst_length=256 \
                            max_widen_bitwidth=512

#pragma HLS INTERFACE s_axilite port=pk     bundle=control
#pragma HLS INTERFACE s_axilite port=coins  bundle=control
#pragma HLS INTERFACE s_axilite port=ct     bundle=control
#pragma HLS INTERFACE s_axilite port=ss     bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  /*
   * ── Local buffers ────────────────────────────────────────────────────
   */
  uint8_t pk_local[KYBER_PUBLICKEYBYTES];
  uint8_t coins_local[KYBER_SYMBYTES];
  uint8_t ct_local[KYBER_CIPHERTEXTBYTES];
  uint8_t ss_local[KYBER_SSBYTES];

  #pragma HLS ARRAY_PARTITION variable=pk_local cyclic factor=8 dim=1
  #pragma HLS ARRAY_PARTITION variable=ct_local cyclic factor=8 dim=1
  #pragma HLS ARRAY_PARTITION variable=coins_local complete dim=1
  #pragma HLS ARRAY_PARTITION variable=ss_local complete dim=1

  unsigned int i;

  memcpy(pk_local, pk, KYBER_PUBLICKEYBYTES);
  memcpy(coins_local, coins, KYBER_SYMBYTES);

  /*
   * ── Phase 2: Encapsulation ───────────────────────────────────────────
   */
  crypto_kem_enc_derand(ct_local, ss_local, pk_local, coins_local);

  /*
   * ── Output write-back ────────────────────────────────────────────────
   */

  memcpy(ct, ct_local, KYBER_CIPHERTEXTBYTES);
  memcpy(ss, ss_local, KYBER_SSBYTES);
}