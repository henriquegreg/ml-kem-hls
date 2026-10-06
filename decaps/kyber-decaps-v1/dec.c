#include <stdint.h>
#include "params.h"
#include "dec.h"
#include "kem.h"

void dec(
    uint8_t ss_out[KYBER_SSBYTES],
    const uint8_t ct_in[KYBER_CIPHERTEXTBYTES],
    const uint8_t sk_in[KYBER_SECRETKEYBYTES]
) {
    #pragma HLS INTERFACE m_axi port=ss_out     offset=slave bundle=gmem_ss    \
                                depth=KYBER_SSBYTES \
                                max_read_burst_length=16
    #pragma HLS INTERFACE m_axi port=ct_in  offset=slave bundle=gmem_ct \
                                depth=KYBER_CIPHERTEXTBYTES \
                                max_read_burst_length=16
    #pragma HLS INTERFACE m_axi port=sk_in     offset=slave bundle=gmem_sk    \
                                depth=KYBER_SECRETKEYBYTES \
                                max_write_burst_length=16

    #pragma HLS INTERFACE s_axilite port=ss_out bundle=control
    #pragma HLS INTERFACE s_axilite port=ct_in  bundle=control
    #pragma HLS INTERFACE s_axilite port=sk_in  bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    uint8_t local_ss[KYBER_SSBYTES];
    uint8_t local_ct[KYBER_CIPHERTEXTBYTES];
    uint8_t local_sk[KYBER_SECRETKEYBYTES];


    for(int i = 0; i < KYBER_CIPHERTEXTBYTES; i++) {
        #pragma HLS PIPELINE II=1
        local_ct[i] = ct_in[i];
    }

    for(int i = 0; i < KYBER_SECRETKEYBYTES; i++) {
        #pragma HLS PIPELINE II=1
        local_sk[i] = sk_in[i];
    }

    crypto_kem_dec(local_ss, local_ct, local_sk);

    for(int i = 0; i < KYBER_SSBYTES; i++) {
        #pragma HLS PIPELINE II=1
        ss_out[i] = local_ss[i];
    }
}