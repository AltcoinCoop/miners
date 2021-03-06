/* XMRig
 * Copyright 2010      Jeff Garzik  <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler       <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones  <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466     <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee    <jayddee246@gmail.com>
 * Copyright 2016      Imran Yusuff <https://github.com/imranyusuff>
 * Copyright 2016-2017 XMRig        <support@xmrig.com>
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CRYPTONIGHT_ARM64_H__
#define __CRYPTONIGHT_ARM64_H__


#if defined(XMRIG_ARM) && !defined(__clang__)
#   include "aligned_malloc.h"
#else
#   include <mm_malloc.h>
#endif


#include "crypto/CryptoNight.h"
#include "crypto/soft_aes.h"


extern "C"
{
#include "crypto/c_keccak.h"
#include "crypto/c_groestl.h"
#include "crypto/c_blake256.h"
#include "crypto/c_jh.h"
#include "crypto/c_skein.h"
}


static inline void do_blake_hash(const void* input, size_t len, char* output) {
    blake256_hash(reinterpret_cast<uint8_t*>(output), static_cast<const uint8_t*>(input), len);
}


static inline void do_groestl_hash(const void* input, size_t len, char* output) {
    groestl(static_cast<const uint8_t*>(input), len * 8, reinterpret_cast<uint8_t*>(output));
}


static inline void do_jh_hash(const void* input, size_t len, char* output) {
    jh_hash(32 * 8, static_cast<const uint8_t*>(input), 8 * len, reinterpret_cast<uint8_t*>(output));
}


static inline void do_skein_hash(const void* input, size_t len, char* output) {
    xmr_skein(static_cast<const uint8_t*>(input), reinterpret_cast<uint8_t*>(output));
}


void (* const extra_hashes[4])(const void *, size_t, char *) = {do_blake_hash, do_groestl_hash, do_jh_hash, do_skein_hash};


static inline __attribute__((always_inline)) __m128i _mm_set_epi64x(const uint64_t a, const uint64_t b)
{
    return vcombine_u64(vcreate_u64(b), vcreate_u64(a));
}


/* this one was not implemented yet so here it is */
static inline __attribute__((always_inline)) uint64_t _mm_cvtsi128_si64(__m128i a)
{
    return vgetq_lane_u64(a, 0);
}


#define EXTRACT64(X) _mm_cvtsi128_si64(X)


static inline uint64_t __umul128(uint64_t a, uint64_t b, uint64_t* hi)
{
    unsigned __int128 r = (unsigned __int128) a * (unsigned __int128) b;
    *hi = r >> 64;
    return (uint64_t) r;
}


// This will shift and xor tmp1 into itself as 4 32-bit vals such as
// sl_xor(a1 a2 a3 a4) = a1 (a2^a1) (a3^a2^a1) (a4^a3^a2^a1)
static inline __m128i sl_xor(__m128i tmp1)
{
    __m128i tmp4;
    tmp4 = _mm_slli_si128(tmp1, 0x04);
    tmp1 = _mm_xor_si128(tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    tmp1 = _mm_xor_si128(tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    tmp1 = _mm_xor_si128(tmp1, tmp4);
    return tmp1;
}


template<uint8_t rcon>
static inline void aes_genkey_sub(__m128i* xout0, __m128i* xout2)
{
//    __m128i xout1 = _mm_aeskeygenassist_si128(*xout2, rcon);
//    xout1  = _mm_shuffle_epi32(xout1, 0xFF); // see PSHUFD, set all elems to 4th elem
//    *xout0 = sl_xor(*xout0);
//    *xout0 = _mm_xor_si128(*xout0, xout1);
//    xout1  = _mm_aeskeygenassist_si128(*xout0, 0x00);
//    xout1  = _mm_shuffle_epi32(xout1, 0xAA); // see PSHUFD, set all elems to 3rd elem
//    *xout2 = sl_xor(*xout2);
//    *xout2 = _mm_xor_si128(*xout2, xout1);
}


template<uint8_t rcon>
static inline void soft_aes_genkey_sub(__m128i* xout0, __m128i* xout2)
{
    __m128i xout1 = soft_aeskeygenassist<rcon>(*xout2);
    xout1  = _mm_shuffle_epi32(xout1, 0xFF); // see PSHUFD, set all elems to 4th elem
    *xout0 = sl_xor(*xout0);
    *xout0 = _mm_xor_si128(*xout0, xout1);
    xout1  = soft_aeskeygenassist<0x00>(*xout0);
    xout1  = _mm_shuffle_epi32(xout1, 0xAA); // see PSHUFD, set all elems to 3rd elem
    *xout2 = sl_xor(*xout2);
    *xout2 = _mm_xor_si128(*xout2, xout1);
}


template<bool SOFT_AES>
static inline void aes_genkey(const __m128i* memory, __m128i* k0, __m128i* k1, __m128i* k2, __m128i* k3, __m128i* k4, __m128i* k5, __m128i* k6, __m128i* k7, __m128i* k8, __m128i* k9)
{
    __m128i xout0 = _mm_load_si128(memory);
    __m128i xout2 = _mm_load_si128(memory + 1);
    *k0 = xout0;
    *k1 = xout2;

    SOFT_AES ? soft_aes_genkey_sub<0x01>(&xout0, &xout2) : soft_aes_genkey_sub<0x01>(&xout0, &xout2);
    *k2 = xout0;
    *k3 = xout2;

    SOFT_AES ? soft_aes_genkey_sub<0x02>(&xout0, &xout2) : soft_aes_genkey_sub<0x02>(&xout0, &xout2);
    *k4 = xout0;
    *k5 = xout2;

    SOFT_AES ? soft_aes_genkey_sub<0x04>(&xout0, &xout2) : soft_aes_genkey_sub<0x04>(&xout0, &xout2);
    *k6 = xout0;
    *k7 = xout2;

    SOFT_AES ? soft_aes_genkey_sub<0x08>(&xout0, &xout2) : soft_aes_genkey_sub<0x08>(&xout0, &xout2);
    *k8 = xout0;
    *k9 = xout2;
}


template<bool SOFT_AES>
static inline void aes_round(__m128i key, __m128i* x0, __m128i* x1, __m128i* x2, __m128i* x3, __m128i* x4, __m128i* x5, __m128i* x6, __m128i* x7)
{
    if (SOFT_AES) {
        *x0 = soft_aesenc(*x0, key);
        *x1 = soft_aesenc(*x1, key);
        *x2 = soft_aesenc(*x2, key);
        *x3 = soft_aesenc(*x3, key);
        *x4 = soft_aesenc(*x4, key);
        *x5 = soft_aesenc(*x5, key);
        *x6 = soft_aesenc(*x6, key);
        *x7 = soft_aesenc(*x7, key);
    }
    else {
        *x0 = vaesmcq_u8(vaeseq_u8(*((uint8x16_t *) x0), key));
        *x1 = vaesmcq_u8(vaeseq_u8(*((uint8x16_t *) x1), key));
        *x2 = vaesmcq_u8(vaeseq_u8(*((uint8x16_t *) x2), key));
        *x3 = vaesmcq_u8(vaeseq_u8(*((uint8x16_t *) x3), key));
        *x4 = vaesmcq_u8(vaeseq_u8(*((uint8x16_t *) x4), key));
        *x5 = vaesmcq_u8(vaeseq_u8(*((uint8x16_t *) x5), key));
        *x6 = vaesmcq_u8(vaeseq_u8(*((uint8x16_t *) x6), key));
        *x7 = vaesmcq_u8(vaeseq_u8(*((uint8x16_t *) x7), key));
    }
}


template<size_t MEM, bool SOFT_AES>
static inline void cn_explode_scratchpad(const __m128i *input, __m128i *output)
{
    __m128i xin0, xin1, xin2, xin3, xin4, xin5, xin6, xin7;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    aes_genkey<SOFT_AES>(input, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    xin0 = _mm_load_si128(input + 4);
    xin1 = _mm_load_si128(input + 5);
    xin2 = _mm_load_si128(input + 6);
    xin3 = _mm_load_si128(input + 7);
    xin4 = _mm_load_si128(input + 8);
    xin5 = _mm_load_si128(input + 9);
    xin6 = _mm_load_si128(input + 10);
    xin7 = _mm_load_si128(input + 11);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8) {
        if (!SOFT_AES) {
            aes_round<SOFT_AES>(_mm_setzero_si128(), &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        }

        aes_round<SOFT_AES>(k0, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k1, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k2, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k3, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k4, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k5, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k6, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k7, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k8, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);

        if (!SOFT_AES) {
            xin0 ^= k9;
            xin1 ^= k9;
            xin2 ^= k9;
            xin3 ^= k9;
            xin4 ^= k9;
            xin5 ^= k9;
            xin6 ^= k9;
            xin7 ^= k9;
        }
        else {
            aes_round<SOFT_AES>(k9, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        }

        _mm_store_si128(output + i + 0, xin0);
        _mm_store_si128(output + i + 1, xin1);
        _mm_store_si128(output + i + 2, xin2);
        _mm_store_si128(output + i + 3, xin3);
        _mm_store_si128(output + i + 4, xin4);
        _mm_store_si128(output + i + 5, xin5);
        _mm_store_si128(output + i + 6, xin6);
        _mm_store_si128(output + i + 7, xin7);
    }
}


template<size_t MEM, bool SOFT_AES>
static inline void cn_implode_scratchpad(const __m128i *input, __m128i *output)
{
    __m128i xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    aes_genkey<SOFT_AES>(output + 2, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    xout0 = _mm_load_si128(output + 4);
    xout1 = _mm_load_si128(output + 5);
    xout2 = _mm_load_si128(output + 6);
    xout3 = _mm_load_si128(output + 7);
    xout4 = _mm_load_si128(output + 8);
    xout5 = _mm_load_si128(output + 9);
    xout6 = _mm_load_si128(output + 10);
    xout7 = _mm_load_si128(output + 11);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
    {
        xout0 = _mm_xor_si128(_mm_load_si128(input + i + 0), xout0);
        xout1 = _mm_xor_si128(_mm_load_si128(input + i + 1), xout1);
        xout2 = _mm_xor_si128(_mm_load_si128(input + i + 2), xout2);
        xout3 = _mm_xor_si128(_mm_load_si128(input + i + 3), xout3);
        xout4 = _mm_xor_si128(_mm_load_si128(input + i + 4), xout4);
        xout5 = _mm_xor_si128(_mm_load_si128(input + i + 5), xout5);
        xout6 = _mm_xor_si128(_mm_load_si128(input + i + 6), xout6);
        xout7 = _mm_xor_si128(_mm_load_si128(input + i + 7), xout7);

        if (!SOFT_AES) {
            aes_round<SOFT_AES>(_mm_setzero_si128(), &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        }

        aes_round<SOFT_AES>(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);

        if (!SOFT_AES) {
            xout0 ^= k9;
            xout1 ^= k9;
            xout2 ^= k9;
            xout3 ^= k9;
            xout4 ^= k9;
            xout5 ^= k9;
            xout6 ^= k9;
            xout7 ^= k9;
        }
        else {
            aes_round<SOFT_AES>(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        }
    }

    _mm_store_si128(output + 4, xout0);
    _mm_store_si128(output + 5, xout1);
    _mm_store_si128(output + 6, xout2);
    _mm_store_si128(output + 7, xout3);
    _mm_store_si128(output + 8, xout4);
    _mm_store_si128(output + 9, xout5);
    _mm_store_si128(output + 10, xout6);
    _mm_store_si128(output + 11, xout7);
}


template<size_t ITERATIONS, size_t MEM, size_t MASK, bool SOFT_AES>
inline void cryptonight_hash(const void *__restrict__ input, size_t size, void *__restrict__ output, cryptonight_ctx *__restrict__ ctx)
{
    keccak(static_cast<const uint8_t*>(input), (int) size, ctx->state0, 200);

    cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) ctx->state0, (__m128i*) ctx->memory);

    const uint8_t* l0 = ctx->memory;
    uint64_t* h0 = reinterpret_cast<uint64_t*>(ctx->state0);

    uint64_t al0 = h0[0] ^ h0[4];
    uint64_t ah0 = h0[1] ^ h0[5];
    __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);

    uint64_t idx0 = h0[0] ^ h0[4];

    for (size_t i = 0; i < ITERATIONS; i++) {
        __m128i cx = _mm_load_si128((__m128i *) &l0[idx0 & MASK]);

        if (SOFT_AES) {
            cx = soft_aesenc(cx, _mm_set_epi64x(ah0, al0));
        }
        else {
            cx = vreinterpretq_m128i_u8(vaesmcq_u8(vaeseq_u8(cx, vdupq_n_u8(0)))) ^ _mm_set_epi64x(ah0, al0);
        }

        _mm_store_si128((__m128i *) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx));
        idx0 = EXTRACT64(cx);
        bx0 = cx;

        uint64_t hi, lo, cl, ch;
        cl = ((uint64_t*) &l0[idx0 & MASK])[0];
        ch = ((uint64_t*) &l0[idx0 & MASK])[1];
        lo = __umul128(idx0, cl, &hi);

        al0 += hi;
        ah0 += lo;

        ((uint64_t*)&l0[idx0 & MASK])[0] = al0;
        ((uint64_t*)&l0[idx0 & MASK])[1] = ah0;

        ah0 ^= ch;
        al0 ^= cl;
        idx0 = al0;
    }

    cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) ctx->memory, (__m128i*) ctx->state0);

    keccakf(h0, 24);
    extra_hashes[ctx->state0[0] & 3](ctx->state0, 200, static_cast<char*>(output));
}


template<size_t ITERATIONS, size_t MEM, size_t MASK, bool SOFT_AES>
inline void cryptonight_double_hash(const void *__restrict__ input, size_t size, void *__restrict__ output, struct cryptonight_ctx *__restrict__ ctx)
{
    keccak((const uint8_t *) input,        (int) size, ctx->state0, 200);
    keccak((const uint8_t *) input + size, (int) size, ctx->state1, 200);

    const uint8_t* l0 = ctx->memory;
    const uint8_t* l1 = ctx->memory + MEM;
    uint64_t* h0 = reinterpret_cast<uint64_t*>(ctx->state0);
    uint64_t* h1 = reinterpret_cast<uint64_t*>(ctx->state1);

    cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
    cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);

    uint64_t al0 = h0[0] ^ h0[4];
    uint64_t al1 = h1[0] ^ h1[4];
    uint64_t ah0 = h0[1] ^ h0[5];
    uint64_t ah1 = h1[1] ^ h1[5];

    __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
    __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);

    uint64_t idx0 = h0[0] ^ h0[4];
    uint64_t idx1 = h1[0] ^ h1[4];

    for (size_t i = 0; i < ITERATIONS; i++) {
        __m128i cx0 = _mm_load_si128((__m128i *) &l0[idx0 & MASK]);
        __m128i cx1 = _mm_load_si128((__m128i *) &l1[idx1 & MASK]);

        if (SOFT_AES) {
            cx0 = soft_aesenc(cx0, _mm_set_epi64x(ah0, al0));
            cx1 = soft_aesenc(cx1, _mm_set_epi64x(ah1, al1));
        }
        else {
            cx0 = vreinterpretq_m128i_u8(vaesmcq_u8(vaeseq_u8(cx0, vdupq_n_u8(0)))) ^ _mm_set_epi64x(ah0, al0);
            cx1 = vreinterpretq_m128i_u8(vaesmcq_u8(vaeseq_u8(cx1, vdupq_n_u8(0)))) ^ _mm_set_epi64x(ah1, al1);
        }

        _mm_store_si128((__m128i *) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
        _mm_store_si128((__m128i *) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));

        idx0 = EXTRACT64(cx0);
        idx1 = EXTRACT64(cx1);

        bx0 = cx0;
        bx1 = cx1;

        uint64_t hi, lo, cl, ch;
        cl = ((uint64_t*) &l0[idx0 & MASK])[0];
        ch = ((uint64_t*) &l0[idx0 & MASK])[1];
        lo = __umul128(idx0, cl, &hi);

        al0 += hi;
        ah0 += lo;

        ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
        ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

        ah0 ^= ch;
        al0 ^= cl;
        idx0 = al0;

        cl = ((uint64_t*) &l1[idx1 & MASK])[0];
        ch = ((uint64_t*) &l1[idx1 & MASK])[1];
        lo = __umul128(idx1, cl, &hi);

        al1 += hi;
        ah1 += lo;

        ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
        ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

        ah1 ^= ch;
        al1 ^= cl;
        idx1 = al1;
    }

    cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
    cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);

    keccakf(h0, 24);
    keccakf(h1, 24);

    extra_hashes[ctx->state0[0] & 3](ctx->state0, 200, static_cast<char*>(output));
    extra_hashes[ctx->state1[0] & 3](ctx->state1, 200, static_cast<char*>(output) + 32);
}

#endif /* __CRYPTONIGHT_ARM64_H__ */
