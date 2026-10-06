#include <stddef.h>
#include <stdint.h>
#include "verify.h"

/*************************************************
* Name:        verify
*
* Description: Compare two arrays for equality in constant time.
*
* Arguments:   const uint8_t *a: pointer to first byte array
*              const uint8_t *b: pointer to second byte array
*              size_t len:       length of the byte arrays
*
* Returns 0 if the byte arrays are equal, 1 otherwise
**************************************************/
int verify(const uint8_t *a, const uint8_t *b, size_t len)
{
  #pragma HLS INLINE off
  size_t i;
  uint8_t r = 0;

  for(i=0;i<len;i++){
  #pragma HLS PIPELINE II=1
    r |= a[i] ^ b[i];
  }

  return (-(uint64_t)r) >> 63;
}

/*************************************************
* Name:        cmov
*
* Description: Copy len bytes from x to r if b is 1;
*              don't modify x if b is 0. Requires b to be in {0,1};
*              assumes two's complement representation of negative integers.
*              Runs in constant time.
*
* Arguments:   uint8_t *r:       pointer to output byte array
*              const uint8_t *x: pointer to input byte array
*              size_t len:       Amount of bytes to be copied
*              uint8_t b:        Condition bit; has to be in {0,1}
**************************************************/
void cmov(uint8_t *r, const uint8_t *x, size_t len, uint8_t b)
{
  #pragma HLS INLINE off
  size_t i;

  /*
   * HLS: the __asm__ volatile fence present in the reference is removed
   * because Vitis HLS synthesises directly to RTL and does not perform
   * the software-compiler optimisations the fence was guarding against.
   * The XOR-mask pattern below is synthesised as a mux with no branch,
   * preserving the constant-time property in the generated RTL.
   */
  b = -b;
  for(i=0;i<len;i++){
    #pragma HLS PIPELINE II=1
    r[i] ^= b & (r[i] ^ x[i]);
  }
}


/*************************************************
* Name:        cmov_int16
*
* Description: Copy input v to *r if b is 1, don't modify *r if b is 0. 
*              Requires b to be in {0,1};
*              Runs in constant time.
*
* Arguments:   int16_t *r:       pointer to output int16_t
*              int16_t v:        input int16_t 
*              uint8_t b:        Condition bit; has to be in {0,1}
**************************************************/
void cmov_int16(int16_t *r, int16_t v, uint16_t b)
{
  #pragma HLS INLINE
  b = -b;
  *r ^= b & ((*r) ^ v);
}