// http://www.math.keio.ac.jp/~matumoto/ver980409.html

// This is the ``Mersenne Twister'' random number generator MT19937, which
// generates pseudorandom integers uniformly distributed in 0..(2^32 - 1)
// starting from any odd seed in 0..(2^32 - 1).  This version is a recode
// by Shawn Cokus (Cokus@math.washington.edu) on March 8, 1998 of a version by
// Takuji Nishimura (who had suggestions from Topher Cooper and Marc Rieffel in
// July-August 1997).
//
// Effectiveness of the recoding (on Goedel2.math.washington.edu, a DEC Alpha
// running OSF/1) using GCC -O3 as a compiler: before recoding: 51.6 sec. to
// generate 300 million random numbers; after recoding: 24.0 sec. for the same
// (i.e., 46.5% of original time), so speed is now about 12.5 million random
// number generations per second on this machine.
//
// According to the URL <http://www.math.keio.ac.jp/~matumoto/emt.html>
// (and paraphrasing a bit in places), the Mersenne Twister is ``designed
// with consideration of the flaws of various existing generators,'' has
// a period of 2^19937 - 1, gives a sequence that is 623-dimensionally
// equidistributed, and ``has passed many stringent tests, including the
// die-hard test of G. Marsaglia and the load test of P. Hellekalek and
// S. Wegenkittl.''  It is efficient in memory usage (typically using 2506
// to 5012 bytes of static data, depending on data type sizes, and the code
// is quite short as well).  It generates random numbers in batches of 624
// at a time, so the caching and pipelining of modern systems is exploited.
// It is also divide- and mod-free.
//
// This library is free software; you can redistribute it and/or modify it
// under the terms of the GNU Library General Public License as published by
// the Free Software Foundation (either version 2 of the License or, at your
// option, any later version).  This library is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY, without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
// the GNU Library General Public License for more details.  You should have
// received a copy of the GNU Library General Public License along with this
// library; if not, write to the Free Software Foundation, Inc., 59 Temple
// Place, Suite 330, Boston, MA 02111-1307, USA.
//
// The code as Shawn received it included the following notice:
//
//   Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.  When
//   you use this, send an e-mail to <matumoto@math.keio.ac.jp> with
//   an appropriate reference to your work.
//
// It would be nice to CC: <Cokus@math.washington.edu> when you write.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rsa.h>

#define N (624)                              // length of state vector
#define M (397)                              // a period parameter
#define K (0x9908B0DFU)                      // a magic constant
#define hiBit(u) ((u)&0x80000000U)           // mask all but highest   bit of u
#define loBit(u) ((u)&0x00000001U)           // mask all but lowest    bit of u
#define loBits(u) ((u)&0x7FFFFFFFU)          // mask     the highest   bit of u
#define mixBits(u, v) (hiBit(u) | loBits(v)) // move hi bit of u to hi bit of v

static uint32_t state[N + 1]; // state vector + 1 extra to not violate ANSI C
static uint32_t *next;        // next random value is computed from here
static int left = -1;         // can *next++ this many times before reloading

void seedMT(uint32_t seed)
{
  //
  // We initialize state[0..(N-1)] via the generator
  //
  //   x_new = (69069 * x_old) mod 2^32
  //
  // from Line 15 of Table 1, p. 106, Sec. 3.3.4 of Knuth's
  // _The Art of Computer Programming_, Volume 2, 3rd ed.
  //
  // Notes (SJC): I do not know what the initial state requirements
  // of the Mersenne Twister are, but it seems this seeding generator
  // could be better.  It achieves the maximum period for its modulus
  // (2^30) iff x_initial is odd (p. 20-21, Sec. 3.2.1.2, Knuth); if
  // x_initial can be even, you have sequences like 0, 0, 0, ...;
  // 2^31, 2^31, 2^31, ...; 2^30, 2^30, 2^30, ...; 2^29, 2^29 + 2^31,
  // 2^29, 2^29 + 2^31, ..., etc. so I force seed to be odd below.
  //
  // Even if x_initial is odd, if x_initial is 1 mod 4 then
  //
  //   the          lowest bit of x is always 1,
  //   the  next-to-lowest bit of x is always 0,
  //   the 2nd-from-lowest bit of x alternates      ... 0 1 0 1 0 1 0 1 ... ,
  //   the 3rd-from-lowest bit of x 4-cycles        ... 0 1 1 0 0 1 1 0 ... ,
  //   the 4th-from-lowest bit of x has the 8-cycle ... 0 0 0 1 1 1 1 0 ... ,
  //    ...
  //
  // and if x_initial is 3 mod 4 then
  //
  //   the          lowest bit of x is always 1,
  //   the  next-to-lowest bit of x is always 1,
  //   the 2nd-from-lowest bit of x alternates      ... 0 1 0 1 0 1 0 1 ... ,
  //   the 3rd-from-lowest bit of x 4-cycles        ... 0 0 1 1 0 0 1 1 ... ,
  //   the 4th-from-lowest bit of x has the 8-cycle ... 0 0 1 1 1 1 0 0 ... ,
  //    ...
  //
  // The generator's potency (min. s>=0 with (69069-1)^s = 0 mod 2^32) is
  // 16, which seems to be alright by p. 25, Sec. 3.2.1.3 of Knuth.  It
  // also does well in the dimension 2..5 spectral tests, but it could be
  // better in dimension 6 (Line 15, Table 1, p. 106, Sec. 3.3.4, Knuth).
  //
  // Note that the random number user does not see the values generated
  // here directly since reloadMT() will always munge them first, so maybe
  // none of all of this matters.  In fact, the seed values made here could
  // even be extra-special desirable if the Mersenne Twister theory says
  // so-- that's why the only change I made is to restrict to odd seeds.
  //

  register uint32_t x = (seed | 1U) & 0xFFFFFFFFU, *s = state;
  register int j;

  for (left = 0, *s++ = x, j = N; --j;
       *s++ = (x *= 69069U) & 0xFFFFFFFFU)
    ;
}

uint32_t reloadMT(void)
{
  register uint32_t *p0 = state, *p2 = state + 2, *pM = state + M, s0, s1;
  register int j;

  if (left < -1)
    seedMT(1U);

  left = N - 1, next = state + 1;

  for (s0 = state[0], s1 = state[1], j = N - M + 1; --j; s0 = s1, s1 = *p2++)
    *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

  for (pM = state, j = M; --j; s0 = s1, s1 = *p2++)
    *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

  s1 = state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
  s1 ^= (s1 >> 11);
  s1 ^= (s1 << 7) & 0x9D2C5680U;
  s1 ^= (s1 << 15) & 0xEFC60000U;
  return (s1 ^ (s1 >> 18));
}

uint32_t randomMT(void)
{
  uint32_t y;

  if (--left < 0)
    return (reloadMT());

  y = *next++;
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9D2C5680U;
  y ^= (y << 15) & 0xEFC60000U;
  return (y ^ (y >> 18));
}

static inline uint32_t bswap32(uint32_t x)
{
  return (((x & 0xff000000u) >> 24) |
          ((x & 0x00ff0000u) >> 8) |
          ((x & 0x0000ff00u) << 8) |
          ((x & 0x000000ffu) << 24));
}

int main(int argc, char *argv[])
{
  uint32_t seed = strtol(argv[1], NULL, 16);
  uint64_t match = strtoll(argv[2], NULL, 16);

  // RSA Public Key 0x136 from Supplier Bootloader binary
  BIGNUM *n = BN_new();
  BN_hex2bn(&n, "de5a5615fdda3b76b4ecd8754228885e7bf11fdd6c8c18ac24230f7f770006cfe60465384e6a5ab4daa3009abc65bff2abb1da1428ce7a925366a14833dcd18183bad61b2c66f0d8b9c4c90bf27fe9d1c55bf2830306a13d4559df60783f5809547ffd364dbccea7a7c2fc32a0357ceba3e932abcac6bd6398894a1a22f63bdc45b5da8b3c4e80f8c097ca7ffd18ff6c78c81e94c016c080ee6c5322e1aeb59d2123dce1e4dd20d0f1cdb017326b4fd813c060e8d2acd62e703341784dca667632233de57db820f149964b3f4f0c785c39e2534a7ae36fd115b9f06457822f8a9b7ce7533777a4fb03610d6b4018ab332be4e7ad2f4ac193040e5a037417bc53");
  BIGNUM *e = BN_new();
  BN_dec2bn(&e, "65537");

  int done = 0;
  while (!done)
  {
    left = -1;
    int j = 0;
    uint32_t current_seed = 0;
    {
      current_seed = seed;
      seed = seed + 2;
    }

    seedMT(current_seed);
    uint32_t rand_data[64];
    unsigned char *rand_data_bytes = (unsigned char *)rand_data;
    for (j = 0; j < 64; j++)
    {
      if (j == 63)
      {
        // The last int has the high bytes replaced with 0200, presumably to force the number to be within bounds for exponentiation/encryption.
        rand_data[j] = bswap32(bswap32(randomMT() & 0xFFFF) + 0x0200);
      }
      else
      {
        rand_data[j] = randomMT();
      }
    }
    // This byte is just straight up set to 0, at 800167d4 in SBOOT, who knows why...
    rand_data_bytes[245] = 0;

    BIGNUM *data_num = BN_lebin2bn(rand_data_bytes, 256, NULL);
    BIGNUM *out = BN_new();
    BN_CTX *ctx = BN_CTX_new();
    BN_mod_exp(out, data_num, e, n, ctx);
    unsigned char rsa_output[256];
    BN_bn2lebinpad(out, rsa_output, 256);

    uint32_t *rsa_output_ints = (uint32_t *)rsa_output;
    if (rsa_output_ints[0] == match)
    {
      done = 1;
      printf("**** FOUND ****\n");
      printf("Seed: %08X\n", current_seed);
      printf("\nKey Data: \n");
      for (j = 0; j < 64; j++)
      {
        printf(" %08X%s", rand_data[j], (j % 4) == 4 ? " " : "");
      }
      printf("\nSeed Data: \n");
      for (j = 0; j < 64; j++)
      {
        printf(" %08X%s", rsa_output_ints[j], (j % 4) == 4 ? " " : "");
      }
      printf("\n");
    }
  }

  return (EXIT_SUCCESS);
}
