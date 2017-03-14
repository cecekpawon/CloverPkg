/*
Copyright (c) 2015-2016, Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the distribution.

3.  Neither the name of the copyright holder(s) nor the names of any contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
  To be used with EDK2
  @cecekpawon Fri Feb 24 08:58:13 2017
*/

#include <Library/Platform/Platform.h>

//#define LZVN_STATUS_TEST      1
#define LZVN_WITH_HEADER        1

#if LZVN_STATUS_TEST
#include "a.h" // sample of encoded with header
#include "b.h" // sample of encoded without header
#include "c.h" // sample of raw decoded
#endif

#define DBG(...) DebugLog (1, __VA_ARGS__)

// LZVN low-level decoder

#define LZVN_STATUS_OK          0
#define LZVN_STATUS_SRC_EMPTY   ENCODE_ERROR (1)
#define LZVN_STATUS_DST_FULL    ENCODE_ERROR (2)
#define LZVN_STATUS_ERROR       ENCODE_ERROR (3)

// ===============================================================
// Types and Constants

#define LZVN_ENCODE_HASH_BITS                                                  \
  14 // number of bits returned by the hash function [10, 16]
#define LZVN_ENCODE_OFFSETS_PER_HASH                                           \
  4 // stored offsets stack for each hash value, MUST be 4
#define LZVN_ENCODE_HASH_VALUES                                                \
  (1 << LZVN_ENCODE_HASH_BITS) // number of entries in hash table
#define LZVN_ENCODE_MAX_DISTANCE                                               \
  0xffff // max match distance we can represent with LZVN encoding, MUST be
         // 0xFFFF
#define LZVN_ENCODE_MIN_MARGIN                                                 \
  8 // min number of bytes required between current and end during encoding,
    // MUST be >= 8
#define LZVN_ENCODE_MAX_LITERAL_BACKLOG                                        \
  400 // if the number of pending literals exceeds this size, emit a long
      // literal, MUST be >= 271

// MARK: - LZVN encode/decode interfaces

//  Minimum source buffer size for compression. Smaller buffers will not be
//  compressed; the lzvn encoder will simply return.
#define LZVN_ENCODE_MIN_SRC_SIZE ((UINTN)8)

//  Maximum source buffer size for compression. Larger buffers will be
//  compressed partially.
#define LZVN_ENCODE_MAX_SRC_SIZE ((UINTN)0xffffffffU)

//  Minimum destination buffer size for compression. No compression will take
//  place if smaller.
#define LZVN_ENCODE_MIN_DST_SIZE ((UINTN)8)

// Work size
#define LZVN_ENCODE_WORK_SIZE                                                  \
  (LZVN_ENCODE_HASH_VALUES * sizeof (LzvnEncodeEntryType))

#define Expect(X, Y) (X)

//  Both the source and destination buffers are represented by a pointer and
//  a length; they are *always* updated in concert using this macro; however
//  many bytes the pointer is advanced, the length is decremented by the same
//  amount. Thus, pointer + length always points to the byte one past the end
//  of the buffer.
#define PTR_LEN_INC(p, l, i) (p += i, l -= i)

//  Update state with current positions and distance, corresponding to the
//  beginning of an instruction in both streams
#define UPDATE_GOOD(S, Src, Dst, D) (S->src = Src, S->dst = Dst, S->d_prev = D)

//#define LZVN_ENCODE_THRESHOLD         4096

// MARK: - Block header objects

#if LZVN_WITH_HEADER
#define LZVN_NO_BLOCK_MAGIC             0x00000000 // 0    (invalid)
#define LZVN_BLOCK_MAGIC                0x6e787662 // bvxn (lzvn compressed)
#define LZVN_ENDOFSTREAM_BLOCK_MAGIC    0x24787662 // bvx$ (end of stream)

/*! @abstract LZVN compressed block header. */
typedef struct {
  //  Magic number, always LZVN_BLOCK_MAGIC.
  UINT32  magic;
  //  Number of decoded (output) bytes.
  UINT32  n_raw_bytes;
  //  Number of encoded (source) bytes.
  UINT32  n_payload_bytes;
} LzvnCompressedBlockHeader;
#endif

/*! @abstract Type of table entry. */
typedef struct {
  INT32   indices[4]; // signed indices in source buffer
  UINT32  values[4];  // corresponding 32-bit values
} LzvnEncodeEntryType;

/*! @abstract Match */
typedef struct {
  INT64   m_begin;  // beginning of match, current position
  INT64   m_end;    // end of match, this is where the next literal would begin
                    // if we emit the entire match
  INT64   M;        // match length M: m_end - m_begin
  INT64   D;        // match distance D
  INT64   K;        // match gain: M - distance storage (L not included)
} LzvnMatchInfo;

/*! @abstract Base encoder state and I/O. */
typedef struct {
  // Encoder I/O

  // Source buffer
  CONST UINT8                 *src;
  // Valid range in source buffer: we can access src[i] for src_begin <= i <
  // src_end. src_begin may be negative.
        INT64                 src_begin;
        INT64                 src_end;
  // Next byte to process in source buffer
        INT64                 src_current;
  // Next byte after the last byte to process in source buffer. We MUST have:
  // src_current_end + 8 <= src_end.
        INT64                 src_current_end;
  // Next byte to encode in source buffer, may be before or after src_current.
        INT64                 src_literal;

  // Next byte to write in destination buffer
        UINT8                 *dst;
  // Valid range in destination buffer: [dst_begin, dst_end - 1]
        UINT8                 *dst_begin;
        UINT8                 *dst_end;

  // Encoder state

  // Pending match
        LzvnMatchInfo         pending;

  // Distance for last emitted match, or 0
        INT64                 d_prev;

  // Hash table used to find matches. Stores LZVN_ENCODE_OFFSETS_PER_HASH 32-bit
  // signed indices in the source buffer, and the corresponding 4-byte values.
  // The number of entries in the table is LZVN_ENCODE_HASH_VALUES.
        LzvnEncodeEntryType   *table;
} LzvnEncoderState;

/*! @abstract Base decoder state. */
typedef struct {
  // Decoder I/O

  // Next byte to read in source buffer
  CONST UINT8   *src;
  // Next byte after source buffer
  CONST UINT8   *src_end;

  // Next byte to write in destination buffer (by decoder)
        UINT8   *dst;
  // Valid range for destination buffer is [dst_begin, dst_end - 1]
        UINT8   *dst_begin;
        UINT8   *dst_end;
  // Next byte to read in destination buffer (modified by caller)
        UINT8   *dst_current;

  // Decoder state

  // Partially expanded match, or 0,0,0.
  // In that case, src points to the next literal to copy, or the next op-code if L==0.
        UINTN   L, M, D;

  // Distance for last emitted match, or 0
        INTN    d_prev;

  // Did we decode end-of-stream?
        INT32   end_of_stream;
} LzvnDecoderState;

//#if defined(_MSC_VER) && !defined(__clang__)
//#  define LZFSE_INLINE __forceinline
//#  define __builtin_expect(X, Y) (X)
//#  define __attribute__(X)
//#  pragma warning(disable : 4068) // warning C4068: unknown pragma
//#  pragma warning(disable : 4146)
//
//// Implement GCC bit scan builtins for MSVC
//#include <intrin.h>
//
//LZFSE_INLINE int __builtin_ctzl(unsigned long val) {
//  unsigned long r = 0;
//  if (_BitScanForward(&r, val)) {
//    return r;
//  }
//  return 32;
//}
//#else
//#  define LZFSE_INLINE static inline __attribute__((__always_inline__))
//#endif

/**
 * BitScanForward
 * @author Gerd Isenberg
 * @param bb bitboard to scan
 * @return index (0..63) of least significant one bit
 *         -1023 if passing zero

INT32
BitScanForward (
  UINT64  bb
) {
   union {
      double d;
      struct {
         UINT32 mantissal : 32;
         UINT32 mantissah : 20;
         UINT32 exponent  : 11;
         UINT32 sign      :  1;
      };
   } ud;

   ud.d = (double)(bb & -bb); // isolated LS1B to double
   return ud.exponent - 1023;
}
*/

/**
 * @param bb bitboard to scan
 * @precondition bb != 0
 * @return index (0..63) of least significant one bit

INT32
BitScanForward (
  UINT64  bb
) {
   UINT32 lsb;

   ASSERT (bb != 0);

   bb &= -bb; // LS1B-Isolation
   lsb = (UINT32)bb | (UINT32)(bb >> 32);
   return (((((((((((UINT32)(bb >> 32) != 0)  * 2)
                 + ((lsb & 0xffff0000) != 0)) * 2)
                 + ((lsb & 0xff00ff00) != 0)) * 2)
                 + ((lsb & 0xf0f0f0f0) != 0)) * 2)
                 + ((lsb & 0xcccccccc) != 0)) * 2)
                 + ((lsb & 0xaaaaaaaa) != 0);
}
*/

/**
 * bitScanForward
 * @author Kim Walisch (2012)
 * @param bb bitboard to scan
 * @precondition bb != 0
 * @return index (0..63) of least significant one bit
 */

CONST INT32 Index64[64] = {
    0, 47,  1, 56, 48, 27,  2, 60,
   57, 49, 41, 37, 28, 16,  3, 61,
   54, 58, 35, 52, 50, 42, 21, 44,
   38, 32, 29, 23, 17, 11,  4, 62,
   46, 55, 26, 59, 40, 36, 15, 53,
   34, 51, 20, 43, 31, 22, 10, 45,
   25, 39, 14, 33, 19, 30,  9, 24,
   13, 18,  8, 12,  7,  6,  5, 63
};

INT32
BitScanForward (
  UINT64  bb
) {
   CONST UINT64 Debruijn64 = 0x03f79d71b4cb0a89UL;

   return Index64[((bb ^ (bb - 1)) * Debruijn64) >> 58];
}

/*! @abstract Load bytes from memory location SRC. */
UINT16
Load2 (
  CONST VOID  *Ptr
) {
  UINT16  Data;

  CopyMem (&Data, Ptr, sizeof Data);

  return Data;
}

UINT32
Load4 (
  CONST VOID  *Ptr
) {
  UINT32 Data;

  CopyMem (&Data, Ptr, sizeof (Data));

  return Data;
}

UINT64
Load8 (
  CONST VOID  *Ptr
) {
  UINT64 Data;

  CopyMem (&Data, Ptr, sizeof (Data));

  return Data;
}

/*! @abstract Store bytes to memory location DST. */
VOID
Store2 (
  VOID    *Ptr,
  UINT16  Data
) {
  CopyMem (Ptr, &Data, sizeof (Data));
}

VOID
Store4 (
  VOID    *Ptr,
  UINT32  Data
) {
  CopyMem (Ptr, &Data, sizeof (Data));
}

VOID
Store8 (
  VOID    *Ptr,
  UINT64  Data
) {
  CopyMem (Ptr, &Data, sizeof (Data));
}

// ===============================================================
// Coarse/fine copy, non overlapping buffers

/*! @abstract Copy at least \p nbytes bytes from \p src to \p dst, by blocks
 * of 8 bytes (may go beyond range). No overlap.
 * @return \p dst + \p nbytes. */
STATIC
UINT8 *
Copy64 (
        UINT8   *Dst,
  CONST UINT8   *Src,
        UINTN   NBytes
) {
  UINTN i;

  for (i = 0; i < NBytes; i += 8) {
    Store8 (Dst + i, Load8 (Src + i));
  }

  return Dst + NBytes;
}

/*! @abstract Copy exactly \p NBytes bytes from \p Src to \p Dst (respects range).
 * No overlap.
 * @return \p Dst + \p NBytes. */
STATIC
UINT8 *
Copy8 (
        UINT8   *Dst,
  CONST UINT8   *Src,
        UINTN   NBytes
) {
  UINTN   i;

  for (i = 0; i < NBytes; i++) {
    Dst[i] = Src[i];
  }

  return Dst + NBytes;
}

/*! @abstract Extracts \p width bits from \p container, starting with \p lsb; if
 * we view \p container as a bit array, we extract \c container[lsb:lsb+width]. */
UINTN
Extract (
  UINTN   Container,
  UINTN   Lsb,
  UINTN   Width
) {
  STATIC CONST UINTN  ContainerWidth = sizeof (Container) * 8;

  //if (
  //  !(BOOLEAN)(
  //    (Lsb < ContainerWidth) ||
  //    ((Width > 0) && (Width <= ContainerWidth)) ||
  //    ((Lsb + Width) <= ContainerWidth)
  //  )
  //) {
  //  return -1;
  //}

  ASSERT (Lsb < ContainerWidth);
  ASSERT ((Width > 0) && (Width <= ContainerWidth));
  ASSERT ((Lsb + Width) <= ContainerWidth);

  if (Width == ContainerWidth) {
    return Container;
  }

  return (Container >> Lsb) & (((UINTN)1 << Width) - 1);
}

  //////////////////////////////////////////////////////////////////////////////
 //                                                                    ENCODER
///////////////////////////////////////////////////////////////////////////////


/*! @abstract Emit (L,0,0) instructions (final literal).
 * We read at most \p L bytes from \p p.
 * @param p input stream
 * @param q1 the first byte after the output buffer.
 * @return pointer to the next output, <= \p q1.
 * @return \p q1 if output is full. In that case, output will be partially invalid.
 */
STATIC
UINT8 *
EmitLiteral (
  CONST UINT8   *P,
        UINT8   *Q,
        UINT8   *Q1,
        UINTN   L
) {
  UINTN   x;

  while (L > 15) {
    x = L < 271 ? L : 271;

    if ((Q + x + 10) >= Q1) {
      goto OUT_FULL;
    }

    Store2 (Q, (UINT16)(0xE0 + ((x - 16) << 8)));

    Q += 2;
    L -= x;
    Q = Copy8 (Q, P, x);
    P += x;
  }

  if (L > 0) {
    if ((Q + L + 10) >= Q1) {
      goto OUT_FULL;
    }

    *Q++ = (UINT8)(0xE0 + L); // 1110LLLL
    Q = Copy8 (Q, P, L);
  }

  return Q;

OUT_FULL:

  return Q1;
}

/*! @abstract Emit (L,M,D) instructions. M>=3.
 * @param p input stream pointing to the beginning of the literal. We read at
 * most \p L+4 bytes from \p p.
 * @param q1 the first byte after the output buffer.
 * @return pointer to the next output, <= \p q1.
 * @return \p q1 if output is full. In that case, output will be partially invalid.
 */
STATIC
UINT8 *
Emit (
  CONST UINT8   *P,
        UINT8   *Q,
        UINT8   *Q1,
        UINTN   L,
        UINTN   M,
        UINTN   D,
        UINTN   D_prev
) {
  UINTN   x;
  UINT32  Literal;

  while (L > 15) {
    x = L < 271 ? L : 271;

    if ((Q + x + 10) >= Q1) {
      goto OUT_FULL;
    }

    Store2 (Q, (UINT16)(0xE0 + ((x - 16) << 8)));

    Q += 2;
    L -= x;
    Q = Copy64 (Q, P, x);
    P += x;
  }

  if (L > 3) {
    if ((Q + L + 10) >= Q1) {
      goto OUT_FULL;
    }

    *Q++ = (UINT8)(0xE0 + L); // 1110LLLL
    Q = Copy64 (Q, P, L);
    P += L;
    L = 0;
  }

  x = M <= (10 - 2 * L) ? M : 10 - 2 * L; // x = min (10-2*L,M)
  M -= x;
  x -= 3; // M = (x+3) + M'    max value for x is 7-2*L

  // Here L<4 literals remaining, we read them here
  Literal = Load4 (P);
  // P is not accessed after this point

  // Relaxed capacity test covering all cases
  if ((Q + 8) >= Q1) {
    goto OUT_FULL;
  }

  if (D == D_prev) {
    if (L == 0) {
      *Q++ = (UINT8)(0xF0 + (x + 3)); // XM!
    } else {
      *Q++ = (UINT8)((L << 6) + (x << 3) + 6); //  LLxxx110
    }

    Store4 (Q, Literal);
    Q += L;
  } else if (D < (2048 - 2 * 256)) {
    // Short dist    D>>8 in 0..5
    *Q++ = (UINT8)((D >> 8) + (L << 6) + (x << 3)); // LLxxxDDD
    *Q++ = D & 0xFF;
    Store4 (Q, Literal);
    Q += L;
  } else if ((D >= (1 << 14)) || (M == 0) || (((x + 3) + M) > 34)) {
    // Long dist
    *Q++ = (UINT8)((L << 6) + (x << 3) + 7);
    Store2 (Q, (UINT16)D);
    Q += 2;
    Store4 (Q, Literal);
    Q += L;
  } else {
    // Medium distance
    x += M;
    M = 0;
    *Q++ = (UINT8)(0xA0 + (x >> 2) + (L << 3));
    Store2 (Q, (UINT16)(D << 2 | (x & 3)));
    Q += 2;
    Store4 (Q, Literal);
    Q += L;
  }

  // Issue remaining match
  while (M > 15) {
    if ((Q + 2) >= Q1) {
      goto OUT_FULL;
    }

    x = M < 271 ? M : 271;
    Store2 (Q, (UINT16)(0xf0 + ((x - 16) << 8)));
    Q += 2;
    M -= x;
  }

  if (M > 0) {
    if ((Q + 1) >= Q1) {
      goto OUT_FULL;
    }

    *Q++ = (UINT8)(0xF0 + M); // M = 0..15
  }

  return Q;

OUT_FULL:
  return Q1;
}

// ===============================================================
// Conversions

/*! @abstract Return 32-bit value to store for offset x. */
STATIC
INT32
OffsetTo32 (
  INT64   x
) {
  return (INT32)x;
}

/*! @abstract Get offset from 32-bit stored value x. */
STATIC
INT64
OffsetFrom32 (
  INT32   x
) {
  return (INT64)x;
}

// ===============================================================
// Hash and Matching

/*! @abstract Get hash in range \c [0,LZVN_ENCODE_HASH_VALUES-1] from 3 bytes in i. */
STATIC
UINT32
Hash3i (
  UINT32  i
) {
  UINT32  H;

  i &= 0xffffff; // truncate to 24-bit input (slightly increases compression ratio)
  H = (i * (1 + (1 << 6) + (1 << 12))) >> 12;
  return H & (LZVN_ENCODE_HASH_VALUES - 1);
}

/*! @abstract Return the number [0, 4] of zero bytes in \p x, starting from the
 * least significant byte. */
STATIC
INT64
TrailingZeroBytes (
  UINT32  x
) {
  return (x == 0) ? 4 : (/* __builtin_ctzl */ BitScanForward (x) >> 3);
}

/*! @abstract Return the number [0, 4] of matching chars between values at
 * \p Src+i and \p Src+j, starting from the least significant byte.
 * Assumes we can read 4 chars from each position. */
STATIC
INT64
NMatch4 (
  CONST UINT8   *Src,
        INT64   i,
        INT64   j
) {
  UINT32  Vi = Load4 (Src + i),
          Vj = Load4 (Src + j);

  return TrailingZeroBytes (Vi ^ Vj);
}

/*! @abstract Check if LBegin, MBegin, M0Begin (M0Begin < MBegin) can be
 * expanded to a match of length at least 3.
 * @param MBegin new string to match.
 * @param M0Begin candidate old string.
 * @param Src source buffer, with valid indices SrcBegin <= i < SrcEnd.
 * (src_begin may be <0)
 * @return If a match can be found, return 1 and set all \p match fields,
 * otherwise return 0.
 * @note \p *match should be 0 before the call. */
STATIC
INT32
LzvnFindMatch (
  CONST UINT8           *Src,
        INT64           SrcBegin,
        INT64           SrcEnd,
        INT64           LBegin,
        INT64           M0Begin,
        INT64           MBegin,
        LzvnMatchInfo   *Match
) {
  INT64 N = NMatch4 (Src, MBegin, M0Begin),
        D, M, MEnd;

  if (N < 3) {
    return 0; // no match
  }

  D = MBegin - M0Begin; // actual distance
  if ((D <= 0) || (D > LZVN_ENCODE_MAX_DISTANCE)) {
    return 0; // distance out of range
  }

  // Expand forward
  MEnd = MBegin + N;
  while ((N == 4) && ((MEnd + 4) < SrcEnd)) {
    N = NMatch4 (Src, MEnd, MEnd - D);
    MEnd += N;
  }

  // Expand backwards over literal
  while (
    (M0Begin > SrcBegin) &&
    (MBegin > LBegin) &&
    (Src[MBegin - 1] == Src[M0Begin - 1])
  ) {
    M0Begin--;
    MBegin--;
  }

  // OK, we keep it, update MATCH
  M = MEnd - MBegin; // match length
  Match->m_begin = MBegin;
  Match->m_end = MEnd;
  Match->K = M - ((D < 0x600) ? 2 : 3);
  Match->M = M;
  Match->D = D;

  return 1; // OK
}

/*! @abstract Same as LzvnFindMatch, but we already know that N bytes do
 *  match (N<=4). */
STATIC
INT32
LzvnFindMatchN (
  CONST UINT8           *Src,
        INT64           SrcBegin,
        INT64           SrcEnd,
        INT64           LBbegin,
        INT64           M0Begin,
        INT64           MBegin,
        INT64           n,
        LzvnMatchInfo   *Match
) {
  INT64   D, M, MEnd;

  // We can skip the first comparison on 4 bytes
  if (n < 3) {
    return 0; // no match
  }

  D = MBegin - M0Begin; // actual distance
  if ((D <= 0) || (D > LZVN_ENCODE_MAX_DISTANCE)) {
    return 0; // distance out of range
  }

  // Expand forward
  MEnd = MBegin + n;
  while ((n == 4) && ((MEnd + 4) < SrcEnd)) {
    n = NMatch4 (Src, MEnd, MEnd - D);
    MEnd += n;
  }

  // Expand backwards over literal
  while (
    (M0Begin > SrcBegin) &&
    (MBegin > LBbegin) &&
    (Src[MBegin - 1] == Src[M0Begin - 1])
  ) {
    M0Begin--;
    MBegin--;
  }

  // OK, we keep it, update MATCH
  M = MEnd - MBegin; // match length
  Match->m_begin = MBegin;
  Match->m_end = MEnd;
  Match->K = M - ((D < 0x600) ? 2 : 3);
  Match->M = M;
  Match->D = D;

  return 1; // OK
}

// ===============================================================
// Encoder Backend

/*! @abstract Emit a match and update State.
 * @return number of bytes written to \p dst. May be 0 if there is no more space
 * in \p dst to emit the match. */
STATIC
INT64
LzvnEmitMatch (
  LzvnEncoderState  *State,
  LzvnMatchInfo     Match
) {
  UINTN   L = (UINTN)(Match.m_begin - State->src_literal),  // literal count
          M = (UINTN)Match.M,                               // match length
          D = (UINTN)Match.D,                               // match distance
          D_prev = (UINTN)State->d_prev; // previously emitted match distance
  INT64   DstUsed;
  UINT8   *Dst = Emit (
                    State->src + State->src_literal, State->dst,
                    State->dst_end, L, M, D, D_prev
                  );

  // Check if DST is full
  if (Dst >= State->dst_end) {
    return 0; // FULL
  }

  // Update State
  DstUsed = Dst - State->dst;
  State->d_prev = Match.D;
  State->dst = Dst;
  State->src_literal = Match.m_end;

  return DstUsed;
}

/*! @abstract Emit a n-bytes literal and update state.
 * @return number of bytes written to \p dst. May be 0 if there is no more space
 * in \p dst to emit the literal. */
STATIC
INT64
LzvnEmitLiteral (
  LzvnEncoderState  *State,
  INT64             N
) {
  UINTN   L = (UINTN)N;
  INT64   DstUsed;
  UINT8   *Dst = EmitLiteral (
                  State->src + State->src_literal, State->dst,
                  State->dst_end, L
                );

  // Check if DST is full
  if (Dst >= State->dst_end) {
    return 0; // FULL
  }

  // Update State
  DstUsed = Dst - State->dst;
  State->dst = Dst;
  State->src_literal += N;

  return DstUsed;
}

/*! @abstract Emit end-of-stream and update State.
 * @return number of bytes written to \p dst. May be 0 if there is no more space
 * in \p dst to emit the instruction. */
STATIC
INT64
LzvnEmitEndOfStream (
  LzvnEncoderState  *State
) {
  // Do we have 8 byte in dst?
  if (State->dst_end < (State->dst + 8)) {
    return 0; // FULL
  }

  // Insert end marker and update State
  Store8 (State->dst, 0x06); // end-of-stream command
  State->dst += 8;

  return 8; // dst_used
}

// ===============================================================
// Encoder Functions

/*! @abstract Initialize encoder table in \p State, uses current I/O parameters. */
STATIC
VOID
LzvnInitTable (
  LzvnEncoderState  *State
) {
  INT64                 Index = -LZVN_ENCODE_MAX_DISTANCE; // max match distance
  UINT32                Value, i;
  LzvnEncodeEntryType   E;

  if (Index < State->src_begin) {
    Index = State->src_begin;
  }

  Value = Load4 (State->src + Index);

  for (i = 0; i < 4; i++) {
    E.indices[i] = OffsetTo32 (Index);
    E.values[i] = Value;
  }

  State->table = AllocatePool (LZVN_ENCODE_HASH_VALUES * sizeof (LzvnEncodeEntryType));

  for (i = 0; i < LZVN_ENCODE_HASH_VALUES; i++) {
    State->table[i] = E; // fill entire table
  }
}

VOID
LzvnEncodeInternal (
  LzvnEncoderState    *State
) {
  CONST LzvnMatchInfo   NO_MATCH = { 0 };

  for (; State->src_current < State->src_current_end; State->src_current++) {
    // Get 4 bytes at src_current
    LzvnEncodeEntryType   UpdatedE; // rotate values, so we will replace the oldest
    LzvnMatchInfo         Incoming = NO_MATCH;
    LzvnEncodeEntryType   E;
    UINT32                Vi = Load4 (State->src + State->src_current),
                          Diffs[4];
    INT32                 k, H;
    INT64                 ik, // index
                          nk; // match byte count

    // Compute new hash H at position I, and push value into position table
    H = Hash3i (Vi); // index of first entry

    // Read table entries for H
    E = State->table[H];

    // Update entry with index=current and value=Vi
    UpdatedE.indices[0] = OffsetTo32 (State->src_current);
    UpdatedE.indices[1] = E.indices[0];
    UpdatedE.indices[2] = E.indices[1];
    UpdatedE.indices[3] = E.indices[2];
    UpdatedE.values[0] = Vi;
    UpdatedE.values[1] = E.values[0];
    UpdatedE.values[2] = E.values[1];
    UpdatedE.values[3] = E.values[2];

    // Do not check matches if still in previously emitted match
    if (State->src_current < State->src_literal) {
      goto AfterEmit;
    }

// Update Best with Candidate if better
#define UPDATE(Best, Candidate)                                                 \
  do {                                                                          \
    if (                                                                        \
      (Candidate.K > Best.K) ||                                                 \
      ((Candidate.K == Best.K) && (Candidate.m_end > (Best.m_end + 1)))         \
    ) {                                                                         \
      Best = Candidate;                                                         \
    }                                                                           \
  } while (0)

// Check Candidate. Keep if better.
#define CHECK_CANDIDATE(ik, nk)                                                 \
  do {                                                                          \
    LzvnMatchInfo   M1;                                                         \
    if (                                                                        \
      LzvnFindMatchN (                                                          \
        State->src, State->src_begin, State->src_end,                           \
        State->src_literal, ik, State->src_current, nk, &M1                     \
      )                                                                         \
    ) {                                                                         \
      UPDATE (Incoming, M1);                                                    \
    }                                                                           \
  } while (0)

// Emit match M. Return if we don't have enough space in the destination buffer
#define EMIT_MATCH(m)                                                           \
  do {                                                                          \
    if (LzvnEmitMatch (State, m) == 0) {                                        \
      return;                                                                   \
    }                                                                           \
  } while (0)

// Emit literal of length L. Return if we don't have enough space in the
// destination buffer
#define EMITLITERAL(l)                                                          \
  do {                                                                          \
    if (LzvnEmitLiteral (State, l) == 0) {                                      \
      return;                                                                   \
    }                                                                           \
  } while (0)

    // Check Candidates in order (closest first)
    for (k = 0; k < 4; k++) {
      Diffs[k] = E.values[k] ^ Vi; // XOR, 0 if equal
    }

    // The values stored in E.xyzw are 32-bit signed indices, extended to signed
    // type INT64
    ik = OffsetFrom32 (E.indices[0]);
    nk = TrailingZeroBytes (Diffs[0]);
    CHECK_CANDIDATE (ik, nk);
    ik = OffsetFrom32 (E.indices[1]);
    nk = TrailingZeroBytes (Diffs[1]);
    CHECK_CANDIDATE (ik, nk);
    ik = OffsetFrom32 (E.indices[2]);
    nk = TrailingZeroBytes (Diffs[2]);
    CHECK_CANDIDATE (ik, nk);
    ik = OffsetFrom32 (E.indices[3]);
    nk = TrailingZeroBytes (Diffs[3]);
    CHECK_CANDIDATE (ik, nk);

    // Check Candidate at previous distance
    if (State->d_prev != 0) {
      LzvnMatchInfo   M1;

      if (
        LzvnFindMatch (
          State->src, State->src_begin, State->src_end,
          State->src_literal, State->src_current - State->d_prev,
          State->src_current, &M1
        )
      ) {
        M1.K = M1.M - 1; // fix K for D_prev
        UPDATE (Incoming, M1);
      }
    }

    // Here we have the Best Candidate in Incoming, may be NO_MATCH

    // If no Incoming match, and literal backlog becomes too high, emit pending
    // match, or literals if there is no pending match
    if (Incoming.M == 0) {
      // at this point, we always have current >= literal
      if ((State->src_current - State->src_literal) >= LZVN_ENCODE_MAX_LITERAL_BACKLOG) {
        if (State->pending.M != 0) {
          EMIT_MATCH (State->pending);
          State->pending = NO_MATCH;
        } else {
          EMITLITERAL (271); // emit long literal (271 is the longest literal size we allow)
        }
      }

      goto AfterEmit;
    }

    if (State->pending.M == 0) {
      // NOTE. Here, we can also emit Incoming right away. It will make the
      // encoder 1.5x faster, at a cost of ~10% lower compression ratio:
      // EMIT_MATCH (Incoming);
      // State->pending = NO_MATCH;

      // No pending match, emit nothing, keep Incoming
      State->pending = Incoming;
    } else {
      // Here we have both Incoming and pending
      if (State->pending.m_end <= Incoming.m_begin) {
        // No overlap: emit pending, keep Incoming
        EMIT_MATCH (State->pending);
        State->pending = Incoming;
      } else {
        // If pending is better, emit pending and discard Incoming.
        // Otherwise, emit Incoming and discard pending.
        if (Incoming.K > State->pending.K) {
          State->pending = Incoming;
        }

        EMIT_MATCH (State->pending);
        State->pending = NO_MATCH;
      }
    }

    AfterEmit:

    // We commit State changes only after we tried to emit instructions, so we
    // can restart in the same State in case dst was full and we quit the loop.
    State->table[H] = UpdatedE;
  } // i loop
  // Do not emit pending match here. We do it only at the end of stream.
}

// ===============================================================
// API entry points

UINTN
LzvnEncodeScratchSize () {
  return LZVN_ENCODE_WORK_SIZE;
}

STATIC
UINTN
LzvnEncodePartial (
        VOID    *Dst,
        UINTN   DstSize,
  CONST VOID    *Src,
        UINTN   SrcSize,
        UINTN   *SrcUsed,
        VOID    *Work
) {
  LzvnEncoderState  State;

  // Min size checks to avoid accessing memory outside buffers.
  if (DstSize < LZVN_ENCODE_MIN_DST_SIZE) {
    *SrcUsed = 0;
    return 0;
  }

  // Max input size check (limit to offsets on UINT32).
  if (SrcSize > LZVN_ENCODE_MAX_SRC_SIZE) {
    SrcSize = LZVN_ENCODE_MAX_SRC_SIZE;
  }

  // Setup encoder State
  SetMem (&State, sizeof (State), 0x00);

  State.src = Src;
  State.src_begin = 0;
  State.src_end = (INT64)SrcSize;
  State.src_literal = 0;
  State.src_current = 0;
  State.dst = Dst;
  State.dst_begin = Dst;
  State.dst_end = (UINT8 *)Dst + DstSize - 8; // reserve 8 bytes for end-of-stream
  State.table = Work;

  // Do not encode if the input buffer is too small. We'll emit a literal instead.
  if (SrcSize >= LZVN_ENCODE_MIN_SRC_SIZE) {
    State.src_current_end = (INT64)SrcSize - LZVN_ENCODE_MIN_MARGIN;
    LzvnInitTable (&State);
    LzvnEncodeInternal (&State);
  }

  // No need to test the return value: src_literal will not be updated on failure,
  // and we will fail later.
  LzvnEmitLiteral (&State, State.src_end - State.src_literal);

  // Restore original size, so end-of-stream always succeeds, and emit it
  State.dst_end = (UINT8 *)Dst + DstSize;
  LzvnEmitEndOfStream (&State);

  *SrcUsed = State.src_literal;

  return (UINTN)(State.dst - State.dst_begin);
}

UINTN
LzvnEncodeBuffer (
        VOID    *Dst,
        UINTN   DstSize,
  CONST VOID    *Src,
        UINTN   SrcSize,
        VOID    *Work
) {
  UINTN   SrcUsed = 0,
          DstUsed = LzvnEncodePartial (Dst, DstSize, Src, SrcSize, &SrcUsed, Work);

  if (SrcUsed != SrcSize) {
    return 0; // could not encode entire input stream = fail
  }

  return DstUsed; // return encoded size
}

EFI_STATUS
LzvnEncode (
        UINT8   **Dst,
        UINTN   *DstSize,
  CONST UINT8   *Src,
        UINTN   SrcSize
) {
#if LZVN_WITH_HEADER
  LzvnCompressedBlockHeader   Header;
                              // need Header + end-of-stream marker
  UINTN                       ExtraSize = 4 + sizeof (LzvnCompressedBlockHeader);
#endif
  UINTN                       Size, ScratchBuffer,
                              ResDstSize = *DstSize = SrcSize;
  UINT8                       *ResDst = NULL;
  EFI_STATUS                  Status = LZVN_STATUS_ERROR;

  // If input is really really small, go directly to uncompressed buffer
  // (because LZVN will refuse to encode it, and we will report a failure)
  if (SrcSize < LZVN_ENCODE_MIN_SRC_SIZE) {
    goto Finish;
  }

#if LZVN_WITH_HEADER
  if (SrcSize <= ExtraSize) {
    goto Finish; // DST is really too small, give up
  }
#endif

  ScratchBuffer = LzvnEncodeScratchSize ();

  ResDst = AllocateZeroPool (SrcSize);

  Size = LzvnEncodeBuffer (
#if LZVN_WITH_HEADER
          ResDst + sizeof (LzvnCompressedBlockHeader),
          ResDstSize - ExtraSize,
#else
          ResDst,
          ResDstSize,
#endif
          Src,
          SrcSize,
          &ScratchBuffer
        );

  if ((Size == 0) || (Size >= SrcSize)) {
    goto Finish; // failed, or no compression, fall back to uncompressed block
  }

#if LZVN_WITH_HEADER
  // If we could encode, setup Header and end-of-stream marker (we left room
  // for them, no need to test)
  Header.magic = LZVN_BLOCK_MAGIC;
  Header.n_raw_bytes = (UINT32)SrcSize;
  Header.n_payload_bytes = (UINT32)Size;
  CopyMem (ResDst, &Header, sizeof (Header));
  Store4 (ResDst + sizeof (LzvnCompressedBlockHeader) + Size, LZVN_ENDOFSTREAM_BLOCK_MAGIC);

  Size += ExtraSize;
#endif

  Status = LZVN_STATUS_OK;

  Finish:

  if (*Dst != NULL) {
    FreePool (*Dst);
    *Dst = NULL;
  }

  *DstSize = 0;

  if (!EFI_ERROR (Status)) {
    *Dst = AllocateCopyPool (Size, ResDst);
    *DstSize = Size;
  }

  if (ResDst != NULL) {
    FreePool (ResDst);
    ResDst = NULL;
  }

  return Status;
}

  //////////////////////////////////////////////////////////////////////////////
 //                                                                    DECODER
///////////////////////////////////////////////////////////////////////////////

VOID
LzvnDecodeInternal (
  LzvnDecoderState  *State
) {
  CONST   UINT8   *SrcPtr = State->src;
          UINT8   *DstPtr = State->dst,
                  Opc;
          UINT16  Opc23;
          UINTN   SrcLen = State->src_end - State->src,
                  DstLen = State->dst_end - State->dst,
                  D = State->d_prev, M, L,
                  OpcLen, i;

  if ((SrcLen == 0) || (DstLen == 0)) {
    return; // empty buffer
  }

  // Do we have a partially expanded match saved in state?
  if ((State->L != 0) || (State->M != 0)) {
    L = State->L;
    M = State->M;
    D = State->D;

    OpcLen = 0; // we already skipped the op

    State->L = State->M = State->D = 0;

    if (M == 0) {
      goto CopyLiteral;
    }

    if (L == 0) {
      goto CopyMatch;
    }

    goto CopyLiteralAndMatch;
  }

  Opc = SrcPtr[0];

  for (;;) {
    switch (Opc) {
      //  ===============================================================
      //  These four Opcodes (sml_d, med_d, lrg_d, and pre_d) encode both a
      //  literal and a match. The bulk of their implementations are shared;
      //  each label here only does the work of setting the Opcode length (not
      //  including any literal bytes), and extracting the literal length, match
      //  length, and match distance (except in pre_d). They then jump into the
      //  shared implementation to actually output the literal and match bytes.
      //
      //  No error checking happens in the first stage, except for ensuring that
      //  the source has enough length to represent the full Opcode before
      //  reading past the first byte.

      //sml_d:

      case   0: case   1: case   2: case   3: case   4: case   5: case   8: case   9:
      case  10: case  11: case  12: case  13: case  16: case  17: case  18: case  19:
      case  20: case  21: case  24: case  25: case  26: case  27: case  28: case  29:
      case  32: case  33: case  34: case  35: case  36: case  37:
      case  40: case  41: case  42: case  43: case  44: case  45: case  48: case  49:
      case  50: case  51: case  52: case  53: case  56: case  57: case  58: case  59:
      case  60: case  61: case  64: case  65: case  66: case  67: case  68: case  69:
      case  72: case  73: case  74: case  75: case  76: case  77:
      case  80: case  81: case  82: case  83: case  84: case  85: case  88: case  89:
      case  90: case  91: case  92: case  93: case  96: case  97: case  98: case  99:
      case 100: case 101: case 104: case 105: case 106: case 107: case 108: case 109:
      case 128: case 129:
      case 130: case 131: case 132: case 133: case 136: case 137: case 138: case 139:
      case 140: case 141: case 144: case 145: case 146: case 147: case 148: case 149:
      case 152: case 153: case 154: case 155: case 156: case 157:
      case 192: case 193: case 194: case 195: case 196: case 197:
      case 200: case 201: case 202: case 203: case 204: case 205:

        UPDATE_GOOD (State, SrcPtr, DstPtr, D);

        // "small distance": This Opcode has the structure LLMMMDDD DDDDDDDD LITERAL
        //  where the length of literal (0-3 bytes) is encoded by the high 2 bits of
        //  the first byte. We first extract the literal length so we know how long
        //  the Opcode is, then check that the source can hold both this Opcode and
        //  at least one byte of the next (because any valid input stream must be
        //  terminated with an eos token).
        OpcLen = 2;

        L = Extract (Opc, 6, 2);
        M = Extract (Opc, 3, 3) + 3;

        //  We need to ensure that the source buffer is long enough that we can
        //  safely read this entire Opcode, the literal that follows, and the first
        //  byte of the next Opcode.  Once we satisfy this requirement, we can
        //  safely unpack the match distance. A check similar to this one is
        //  present in all the Opcode implementations.
        if (SrcLen <= (OpcLen + L)) {
          return; // source truncated
        }

        D = Extract (Opc, 0, 3) << 8 | SrcPtr[1];

        goto CopyLiteralAndMatch;

      //med_d:

      case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167: case 168: case 169:
      case 170: case 171: case 172: case 173: case 174: case 175: case 176: case 177: case 178: case 179:
      case 180: case 181: case 182: case 183: case 184: case 185: case 186: case 187: case 188: case 189:
      case 190: case 191:

        UPDATE_GOOD (State, SrcPtr, DstPtr, D);

        //  "medium distance": This is a minor variant of the "small distance"
        //  encoding, where we will now use two extra bytes instead of one to encode
        //  the restof the match length and distance. This allows an extra two bits
        //  for the match length, and an extra three bits for the match distance. The
        //  full structure of the Opcode is 101LLMMM DDDDDDMM DDDDDDDD LITERAL.
        OpcLen = 3;

        L = Extract (Opc, 3, 2);

        if (SrcLen <= (OpcLen + L)) {
          return; // source truncated
        }

        Opc23 = Load2 (&SrcPtr[1]);

        M = (UINTN)((Extract (Opc, 0, 3) << 2 | Extract (Opc23, 0, 2)) + 3);
        D = Extract (Opc23, 2, 14);

        goto CopyLiteralAndMatch;

      //lrg_d:

      case   7:
      case  15:
      case  23:
      case  31: case  39:
      case  47:
      case  55:
      case  63:
      case  71: case  79:
      case  87:
      case  95:
      case 103:
      case 111:
      case 135:
      case 143:
      case 151: case 159:
      case 199:
      case 207:

        UPDATE_GOOD (State, SrcPtr, DstPtr, D);

        //  "large distance": This is another variant of the "small distance"
        //  encoding, where we will now use two extra bytes to encode the match
        //  distance, which allows distances up to 65535 to be represented. The full
        //  structure of the Opcode is LLMMM111 DDDDDDDD DDDDDDDD LITERAL.
        OpcLen = 3;

        L = Extract (Opc, 6, 2);
        M = Extract (Opc, 3, 3) + 3;

        if (SrcLen <= (OpcLen + L)) {
          return; // source truncated
        }

        D = Load2 (&SrcPtr[1]);

        goto CopyLiteralAndMatch;

      //pre_d:

      case  70: case  78:
      case  86:
      case  94:
      case 102:
      case 110:
      case 134:
      case 142:
      case 150: case 158:
      case 198:
      case 206:

        UPDATE_GOOD (State, SrcPtr, DstPtr, D);
        //  "previous distance": This Opcode has the structure LLMMM110, where the
        //  length of the literal (0-3 bytes) is encoded by the high 2 bits of the
        //  first byte. We first extract the literal length so we know how long
        //  the Opcode is, then check that the source can hold both this Opcode and
        //  at least one byte of the next (because any valid input stream must be
        //  terminated with an eos token).
        OpcLen = 1;

        L = Extract (Opc, 6, 2);
        M = Extract (Opc, 3, 3) + 3;

        if (SrcLen <= (OpcLen + L)) {
          return; // source truncated
        }

        goto CopyLiteralAndMatch;

        CopyLiteralAndMatch:
        //  Common implementation of writing data for Opcodes that have both a
        //  literal and a match. We begin by advancing the source pointer past
        //  the Opcode, so that it points at the first literal byte (if L
        //  is non-zero; otherwise it points at the next Opcode).
        PTR_LEN_INC (SrcPtr, SrcLen, OpcLen);

        //  Now we copy the literal from the source pointer to the destination.
        if (Expect (DstLen >= 4 && SrcLen >= 4, 1)) {
          //  The literal is 0-3 bytes; if we are not near the end of the buffer,
          //  we can safely just do a 4 byte copy (which is guaranteed to cover
          //  the complete literal, and may include some other bytes as well).
          Store4 (DstPtr, Load4 (SrcPtr));
        } else if (L <= DstLen) {
          //  We are too close to the end of either the input or output stream
          //  to be able to safely use a four-byte copy, but we will not exhaust
          //  either stream (we already know that the source will not be
          //  exhausted from checks in the individual Opcode implementations,
          //  and we just tested that DstLen > L). Thus, we need to do a
          //  byte-by-byte copy of the literal. This is slow, but it can only ever
          //  happen near the very end of a buffer, so it is not an important case to
          //  optimize.
          for (i = 0; i < L; ++i) {
            DstPtr[i] = SrcPtr[i];
          }
        } else {
          // Destination truncated: fill DST, and store partial match

          // Copy partial literal
          for (i = 0; i < DstLen; ++i) {
            DstPtr[i] = SrcPtr[i];
          }

          // Save state
          State->src = SrcPtr + DstLen;
          State->dst = DstPtr + DstLen;
          State->L = L - DstLen;
          State->M = M;
          State->D = D;

          return; // destination truncated
        }

        //  Having completed the copy of the literal, we advance both the source
        //  and destination pointers by the number of literal bytes.
        PTR_LEN_INC (DstPtr, DstLen, L);
        PTR_LEN_INC (SrcPtr, SrcLen, L);

        //  Check if the match distance is valid; matches must not reference
        //  bytes that preceed the start of the output buffer, nor can the match
        //  distance be zero.
        if ((D > (UINTN)(DstPtr - State->dst_begin)) || (D == 0)) {
          goto InvalidMatchDistance;
        }

        CopyMatch:
        //  Now we copy the match from DstPtr - D to DstPtr. It is important to keep
        //  in mind that we may have D < M, in which case the source and destination
        //  windows overlap in the copy. The semantics of the match copy are *not*
        //  those of memmove( ); if the buffers overlap it needs to behave as though
        //  we were copying byte-by-byte in increasing address order. If, for example,
        //  D is 1, the copy operation is equivalent to:
        //
        //      memset(DstPtr, DstPtr[-1], M);
        //
        //  i.e. it splats the previous byte. This means that we need to be very
        //  careful about using wide loads or stores to perform the copy operation.
        if (Expect ((DstLen >= (M + 7)) && (D >= 8), 1)) {
          //  We are not near the end of the buffer, and the match distance
          //  is at least eight. Thus, we can safely loop using eight byte
          //  copies. The last of these may slop over the intended end of
          //  the match, but this is OK because we know we have a safety bound
          //  away from the end of the destination buffer.
          for (i = 0; i < M; i += 8) {
            Store8 (&DstPtr[i], Load8 (&DstPtr[i - D]));
          }
        } else if (M <= DstLen) {
          //  Either the match distance is too small, or we are too close to
          //  the end of the buffer to safely use eight byte copies. Fall back
          //  on a simple byte-by-byte implementation.
          for (i = 0; i < M; ++i) {
            DstPtr[i] = DstPtr[i - D];
          }
        } else {
          // Destination truncated: fill DST, and store partial match

          // Copy partial match
          for (i = 0; i < DstLen; ++i) {
            DstPtr[i] = DstPtr[i - D];
          }

          // Save state
          State->src = SrcPtr;
          State->dst = DstPtr + DstLen;
          State->L = 0;
          State->M = M - DstLen;
          State->D = D;

          return; // destination truncated
        }

        //  Update the destination pointer and length to account for the bytes
        //  written by the match, then load the next Opcode byte and branch to
        //  the appropriate implementation.
        PTR_LEN_INC (DstPtr, DstLen, M);

        Opc = SrcPtr[0];

        break;

      // ===============================================================
      //  Opcodes representing only a match (no literal).
      //  These two Opcodes (lrg_m and sml_m) encode only a match. The match
      //  distance is carried over from the previous Opcode, so all they need
      //  to encode is the match length. We are able to reuse the match copy
      //  sequence from the literal and match Opcodes to perform the actual
      //  copy implementation.

      //sml_m:

      case 241: case 242: case 243: case 244: case 245: case 246: case 247: case 248: case 249:
      case 250: case 251: case 252: case 253: case 254: case 255:

        UPDATE_GOOD (State, SrcPtr, DstPtr, D);

        //  "small match": This Opcode has no literal, and uses the previous match
        //  distance (i.e. it encodes only the match length), in a single byte as
        //  1111MMMM.
        OpcLen = 1;
        if (SrcLen <= OpcLen) {
          return; // source truncated
        }

        M = Extract (Opc, 0, 4);

        PTR_LEN_INC (SrcPtr, SrcLen, OpcLen);

        goto CopyMatch;

      //lrg_m:

      case 240:

        UPDATE_GOOD (State, SrcPtr, DstPtr, D);

        //  "large match": This Opcode has no literal, and uses the previous match
        //  distance (i.e. it encodes only the match length). It is encoded in two
        //  bytes as 11110000 MMMMMMMM.  Because matches smaller than 16 bytes can
        //  be represented by sml_m, there is an implicit bias of 16 on the match
        //  length; the representable values are [16,271].
        OpcLen = 2;
        if (SrcLen <= OpcLen) {
          return; // source truncated
        }

        M = SrcPtr[1] + 16;

        PTR_LEN_INC (SrcPtr, SrcLen, OpcLen);

        goto CopyMatch;

      // ===============================================================
      //  Opcodes representing only a literal (no match).
      //  These two Opcodes (lrg_l and sml_l) encode only a literal.  There is no
      //  match length or match distance to worry about (but we need to *not*
      //  touch D, as it must be preserved between Opcodes).

      //sml_l:

      case 225: case 226: case 227: case 228: case 229:
      case 230: case 231: case 232: case 233: case 234: case 235: case 236: case 237: case 238: case 239:

        UPDATE_GOOD (State, SrcPtr, DstPtr, D);

        //  "small literal": This Opcode has no match, and encodes only a literal
        //  of length up to 15 bytes. The format is 1110LLLL LITERAL.
        OpcLen = 1;

        L = Extract (Opc, 0, 4);

        goto CopyLiteral;

      //lrg_l:

      case 224:

        UPDATE_GOOD (State, SrcPtr, DstPtr, D);

        //  "large literal": This Opcode has no match, and uses the previous match
        //  distance (i.e. it encodes only the match length). It is encoded in two
        //  bytes as 11100000 LLLLLLLL LITERAL.  Because literals smaller than 16
        //  bytes can be represented by sml_l, there is an implicit bias of 16 on
        //  the literal length; the representable values are [16,271].
        OpcLen = 2;

        if (SrcLen <= 2) {
          return; // source truncated
        }

        L = SrcPtr[1] + 16;

        goto CopyLiteral;

        CopyLiteral:
        //  Check that the source buffer is large enough to hold the complete
        //  literal and at least the first byte of the next Opcode. If so, advance
        //  the source pointer to point to the first byte of the literal and adjust
        //  the source length accordingly.
        if (SrcLen <= (OpcLen + L)) {
          return; // source truncated
        }

        PTR_LEN_INC (SrcPtr, SrcLen, OpcLen);

        //  Now we copy the literal from the source pointer to the destination.
        if ((DstLen >= (L + 7)) && (SrcLen >= (L + 7))) {
          //  We are not near the end of the source or destination buffers; thus
          //  we can safely copy the literal using wide copies, without worrying
          //  about reading or writing past the end of either buffer.
          for (i = 0; i < L; i += 8) {
            Store8 (&DstPtr[i], Load8 (&SrcPtr[i]));
          }
        } else if (L <= DstLen) {
          //  We are too close to the end of either the input or output stream
          //  to be able to safely use an eight-byte copy. Instead we copy the
          //  literal byte-by-byte.
          for (i = 0; i < L; ++i) {
            DstPtr[i] = SrcPtr[i];
          }
        } else {
          // Destination truncated: fill DST, and store partial match

          // Copy partial literal
          for (i = 0; i < DstLen; ++i) {
            DstPtr[i] = SrcPtr[i];
          }

          // Save state
          State->src = SrcPtr + DstLen;
          State->dst = DstPtr + DstLen;
          State->L = L - DstLen;
          State->M = 0;
          State->D = D;

          return; // destination truncated
        }

        //  Having completed the copy of the literal, we advance both the source
        //  and destination pointers by the number of literal bytes.
        PTR_LEN_INC (DstPtr, DstLen, L);
        PTR_LEN_INC (SrcPtr, SrcLen, L);

        //  Load the first byte of the next Opcode, and jump to its implementation.
        Opc = SrcPtr[0];

        break;

      // ===============================================================
      // Other Opcodes

      //nop:

      case 14:
      case 22:

        UPDATE_GOOD (State, SrcPtr, DstPtr, D);

        OpcLen = 1;
        if (SrcLen <= OpcLen) {
          return; // source truncated
        }

        PTR_LEN_INC (SrcPtr, SrcLen, OpcLen);

        Opc = SrcPtr[0];

        break;

      //eos:

      case 6:

        OpcLen = 8;
        if (SrcLen < OpcLen) {
          return; // source truncated (here we don't need an extra byte for next op code)
        }

        PTR_LEN_INC (SrcPtr, SrcLen, OpcLen);
        State->end_of_stream = 1;
        UPDATE_GOOD (State, SrcPtr, DstPtr, D);

        return; // end-of-stream

      // ===============================================================
      // Return on error

      //udef:

      case  30: case  38:
      case  46:
      case  54:
      case  62:
      case 112: case 113: case 114: case 115: case 116: case 117: case 118: case 119:
      case 120: case 121: case 122: case 123: case 124: case 125: case 126: case 127: case 208: case 209:
      case 210: case 211: case 212: case 213: case 214: case 215: case 216: case 217: case 218: case 219:
      case 220: case 221: case 222: case 223:

        InvalidMatchDistance:

        return; // we already updated state
    }
  }
}

EFI_STATUS
LzvnDecode (
        UINT8   **Dst,
        UINTN   *DstSize,
  CONST UINT8   *Src,
        UINTN   SrcSize
) {
  LzvnDecoderState    State; // Init LZVN decoder state
  UINTN               CompressedSize = SrcSize,
                      NRawBytes = 0, NPayloadBytes = 0,
                      ResDstSize = *DstSize,
                      SrcUsed = 0, DstUsed = 0;
  UINT8               *ResDst = *Dst;
  EFI_STATUS          Status = LZVN_STATUS_ERROR;

#if LZVN_WITH_HEADER
  if (!ResDstSize) {
    LzvnCompressedBlockHeader   *Header = (LzvnCompressedBlockHeader *) Src;
    UINT32                      EndBlockMagic = Load4 (Src + (SrcSize - sizeof (UINT32)));
    //UINT32                    EndBlockMagic = Load4 (Src + sizeof (LzvnCompressedBlockHeader) + CompressedSize);

    if (
      (Header->magic == LZVN_BLOCK_MAGIC) &&
      (Header->n_raw_bytes) &&
      (Header->n_payload_bytes) &&
      (EndBlockMagic == LZVN_ENDOFSTREAM_BLOCK_MAGIC)
    ) {
      CompressedSize = NPayloadBytes = Header->n_payload_bytes;
      ResDstSize = NRawBytes = Header->n_raw_bytes;

      Src += sizeof (LzvnCompressedBlockHeader);

      DBG (
        "Magic: %lx, NRawBytes: %lx, NPayloadBytes: %lx, EndBlockMagic: %lx\n",
        Header->magic, Header->n_raw_bytes, Header->n_payload_bytes, EndBlockMagic
      );
    }
  }
#endif

  if (ResDst != NULL) {
    FreePool (ResDst);
  }

  if (!CompressedSize || !ResDstSize) {
    goto Finish;
  }

  ResDst = AllocatePool (ResDstSize);

  SetMem (&State, sizeof(State), 0x00);

  State.src = Src;
  State.src_end = State.src + CompressedSize;

  if (NPayloadBytes && ((UINTN)(State.src_end - State.src) > NPayloadBytes)) {
    State.src_end = State.src + NPayloadBytes; // limit to payload bytes
  }

  State.dst = ResDst;
  State.dst_begin = State.dst;
  State.dst_end = State.dst + ResDstSize;

  if (NRawBytes && ((UINTN)(State.dst_end - State.dst) > NRawBytes)) {
    State.dst_end = State.dst + NRawBytes; // limit to raw bytes
  }

  State.end_of_stream = 0;

  // Run LZVN decoder
  LzvnDecodeInternal (&State);

  // Update our state
  SrcUsed = State.src - Src;
  DstUsed = State.dst - ResDst;

  if (!SrcUsed || !DstUsed || !State.end_of_stream) {
    goto Finish;
  }

  Status = LZVN_STATUS_OK;

  DBG ("Status: %d, SrcUsed: %d, DstUsed: %d\n", Status, SrcUsed, DstUsed);

  *DstSize = ResDstSize;
  *Dst = ResDst;

  Finish:

  return Status;
}

// End of LZVN Lib

// Test: Draw encoded logo into center of screen

#if LZVN_STATUS_TEST
#define APPLE_LOGO_WIDTH  84
#define APPLE_LOGO_HEIGHT 103

VOID
DrawLogo (
  UINT8   *Logo,
  UINTN   LogoSize
) {
  UINT8     *ClutData = (UINT8 *)AppleLogoClut;
  EG_IMAGE  *LogoImage = CreateImage (APPLE_LOGO_WIDTH, APPLE_LOGO_HEIGHT, FALSE);

  if ((LogoImage != NULL) && (LogoImage->Width == APPLE_LOGO_WIDTH) && (LogoImage->Height == APPLE_LOGO_HEIGHT)) {
    EG_PIXEL  *Pixel = (EG_PIXEL *)LogoImage->PixelData;
    UINTN     i;

    for (i = 0; i < LogoSize; i++, Pixel++, Logo++) {
      UINTN   index = *Logo * 3;

      Pixel->r = ClutData[index + 0];
      Pixel->g = ClutData[index + 1];
      Pixel->b = ClutData[index + 2];
      Pixel->a = 0xFF;
    }

    DrawImageArea (LogoImage, 0, 0, 0, 0, (UGAWidth - APPLE_LOGO_WIDTH) >> 1, (UGAHeight - APPLE_LOGO_HEIGHT) >> 1);

    FreeImage (LogoImage);
  }
}
#endif

VOID
hehe () {
#if LZVN_STATUS_TEST
  UINT8       //*Dst = NULL, *Src = (UINT8 *)AppleLogoPacked;
              *Dst = NULL, *Src = (UINT8 *)a_enc;
  //UINTN     SrcSize = ARRAY_SIZE (AppleLogoPacked), DstSize = APPLE_LOGO_WIDTH * APPLE_LOGO_HEIGHT;
  UINTN       SrcSize = ARRAY_SIZE (a_enc), DstSize = 0;
  EFI_STATUS  Status;

  //SaveFile (SelfRootDir, L"AppleLogo.RAW", Dst, DstSize);

  Status = LzvnDecode (&Dst, &DstSize, Src, SrcSize);

  if (!EFI_ERROR (Status)) {
    DrawLogo (Dst, DstSize);
  }
#endif
}

VOID
hehe2 () {
#if LZVN_STATUS_TEST
  UINTN       DstSize, SrcSize = ARRAY_SIZE (AppleLogo_RAW);
  UINT8       *Dst, *Src = (UINT8 *)AppleLogo_RAW;
  EFI_STATUS  Status;

  Status = LzvnEncode (&Dst, &DstSize, Src, SrcSize);

  DBG ("Encoding Status: %d\n", Status);

  if (!EFI_ERROR (Status)) {
    UINT8   *Logo = NULL;
    UINTN   LogoSize = 0;

    Status = LzvnDecode (&Logo, &LogoSize, Dst, DstSize);

    DBG ("Decoding Status: %d\n", Status);

    if (!EFI_ERROR (Status)) {
      DrawLogo (Logo, LogoSize);
    }

    //SaveFile (SelfRootDir, L"a.enc", Dst, DstSize);
  }
#endif
}
