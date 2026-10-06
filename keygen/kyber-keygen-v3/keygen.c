#include <string.h>
#include <stdint.h>
#include "params.h"
#include "keygen.h"
#include "kem.h"

void keygen(const uint8_t coins[2 * KYBER_SYMBYTES],
                uint8_t       pk[KYBER_PUBLICKEYBYTES],
                uint8_t       sk[KYBER_SECRETKEYBYTES])
{
  /*
   * ── Interface pragmas ────────────────────────────────────────────────
   *
   * m_axi: AXI4 master ports connecting to HBM/DDR on the Alveo U50.
   *   • offset=slave  – the base address of each buffer is passed through
   *                     the AXI4-Lite control slave at runtime by the host.
   *   • bundle        – each port is placed on its own AXI channel so HLS
   *                     can issue independent transactions in parallel.
   *   • depth         – static size hint used by HLS for burst inference;
   *                     must match the actual allocation in the host driver.
   *   • max_read/write_burst_length=16 – 16-beat AXI bursts (256 bytes at
   *                     64-bit width) balance latency and BW on HBM.
   *
   * s_axilite: AXI4-Lite slave for the control/status register block.
   *   All scalar arguments (base addresses) and the return value (done
   *   flag) are mapped here.  The host starts the kernel by writing
   *   ap_start and polls ap_done.
   */
#pragma HLS INTERFACE m_axi port=coins  offset=slave bundle=gmem_coins \
                            depth=(2*KYBER_SYMBYTES) \
                            max_read_burst_length=64  \
                            max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=pk     offset=slave bundle=gmem_pk    \
                            depth=KYBER_PUBLICKEYBYTES \
                            max_read_burst_length=64  \
                            max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=sk     offset=slave bundle=gmem_sk    \
                            depth=KYBER_SECRETKEYBYTES \
                            max_read_burst_length=64  \
                            max_write_burst_length=64

#pragma HLS INTERFACE s_axilite port=coins  bundle=control
#pragma HLS INTERFACE s_axilite port=pk     bundle=control
#pragma HLS INTERFACE s_axilite port=sk     bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  /*
   * ── Local buffers ────────────────────────────────────────────────────
  */
  uint8_t coins_local[2 * KYBER_SYMBYTES];
  uint8_t pk_local[KYBER_PUBLICKEYBYTES];
  uint8_t sk_local[KYBER_SECRETKEYBYTES];

  unsigned int i;

  memcpy(coins_local, coins, 2 * KYBER_SYMBYTES);

  /*
   * ── Phase 2: key generation ──────────────────────────────────────────
   *
   * Calls the unmodified CCA-secure keygen from kem.c.
   * Call chain:
   *   crypto_kem_keypair_derand()
   *     └─ indcpa_keypair_derand()          [indcpa.c]
   *          ├─ hash_g / sha3_512()         [fips202.c]
   *          ├─ gen_matrix()                [indcpa.c]
   *          │    └─ xof_absorb / squeeze   [symmetric-shake.c → fips202.c]
   *          ├─ poly_getnoise_eta1() ×2K    [poly.c → cbd.c]
   *          ├─ polyvec_ntt() ×2            [polyvec.c → ntt.c]
   *          ├─ polyvec_basemul_acc_montgomery() ×K  [polyvec.c]
   *          ├─ polyvec_add / reduce        [polyvec.c]
   *          └─ pack_pk / pack_sk           [indcpa.c]
   *     ├─ hash_h / sha3_256(pk)            [fips202.c]
   *     └─ memcpy z from coins              [kem.c]
   */
  crypto_kem_keypair_derand(pk_local, sk_local, coins_local);

  memcpy(pk, pk_local, KYBER_PUBLICKEYBYTES);
  memcpy(sk, sk_local, KYBER_SECRETKEYBYTES);
}