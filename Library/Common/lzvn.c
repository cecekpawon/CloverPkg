//
//  lzvn.c
//
//  Based on works from Pike R. Alpha and AnV Software (Andy Vandijck).
//  Converted to C by MinusZwei on 9/14/14.
//
//  No dogs allowed.
//

#include <Library/Platform/Platform.h>

#define DBG(...) DebugLog(1, __VA_ARGS__)

#define DEBUG_STATE_ENABLED  0

#if DEBUG_STATE_ENABLED
#define DEBUG_STATE(...) DBG(__VA_ARGS__)
#else
#define DEBUG_STATE(...)
#endif

#define EFI_MAX_ADDRESS   0xFFFFFFFFFFFFFFFF

#define APPLE_LOGO_WIDTH  84
#define APPLE_LOGO_HEIGHT 103

STATIC UINT8 AppleLogoBlackPacked[ 912 ] = {
  0x68, 0x01, 0x00, 0xF0, 0xFF, 0xF0, 0xB7, 0xE4, 0x01, 0x02, 0x01, 0x00, 0xFA, 0xF0, 0x33, 0xE7, 0x03, 0x04, 0x05, 0x06, 0x06, 0x07, 0x00, 0xFA, 0xF0, 0x30, 0xE4, 0x08, 0x07, 0x05, 0x06, 0xF4,
  0x38, 0x54, 0xF0, 0x31, 0x58, 0x52, 0x09, 0x38, 0x54, 0xF0, 0x31, 0xC8, 0x01, 0x01, 0x0A, 0x06, 0xF4, 0x38, 0x54, 0xF0, 0x2E, 0x68, 0xA5, 0x03, 0x00, 0x01, 0x68, 0xA5, 0x02, 0xF0, 0x2D, 0xC8,
  0x01, 0x00, 0x0B, 0x06, 0xF7, 0x39, 0xF8, 0xF0, 0x2C, 0x38, 0x53, 0xF3, 0x6E, 0x0C, 0xF0, 0x2D, 0x6E, 0x0D, 0xF4, 0x9E, 0x06, 0x0E, 0xF0, 0x2E, 0x9E, 0x08, 0x0C, 0xF6, 0x9E, 0x06, 0x0D, 0xF0,
  0x2D, 0x9E, 0x00, 0x0E, 0xF7, 0x6E, 0x0A, 0xF0, 0x2C, 0x6E, 0x01, 0xF5, 0x9E, 0x06, 0x0B, 0xF0, 0x2D, 0x9E, 0x00, 0x04, 0xF7, 0x6E, 0x0F, 0xF0, 0x2C, 0x38, 0xA7, 0xF5, 0x39, 0x4E, 0xF0, 0x39,
  0x3B, 0x44, 0xF0, 0x2C, 0x39, 0xF5, 0xF3, 0x38, 0xFA, 0xF0, 0x2B, 0x38, 0xFB, 0xF4, 0xCE, 0x0A, 0x08, 0x00, 0xF0, 0x30, 0x6E, 0x03, 0xF3, 0x38, 0x53, 0xF0, 0x2D, 0x9E, 0x00, 0x07, 0xF4, 0x6E,
  0x04, 0xF0, 0x2F, 0x38, 0x54, 0xC8, 0x01, 0x05, 0x03, 0x00, 0xF0, 0x34, 0x20, 0x54, 0x98, 0xA5, 0x05, 0x07, 0xF0, 0x32, 0x20, 0x54, 0xC8, 0x52, 0x05, 0x10, 0x0B, 0xF0, 0x37, 0x01, 0xF8, 0x98,
  0x50, 0x02, 0x02, 0xF0, 0x28, 0xE4, 0x02, 0x07, 0x04, 0x0F, 0x00, 0x01, 0x9D, 0xCB, 0x10, 0x07, 0xFF, 0xE4, 0x08, 0x0B, 0x04, 0x0F, 0x11, 0x09, 0xE4, 0x06, 0x0F, 0x04, 0x09, 0x3B, 0x50, 0xF0,
  0x0C, 0xC8, 0x01, 0x0B, 0x10, 0x06, 0xF6, 0x99, 0x93, 0x05, 0x04, 0xF9, 0xB1, 0x47, 0x1A, 0x08, 0x0B, 0x10, 0x01, 0x9D, 0xF0, 0x0C, 0x0E, 0xF0, 0x0A, 0x2F, 0x68, 0x07, 0x38, 0x01, 0xF1, 0x68,
  0x35, 0x05, 0xF3, 0x6C, 0x49, 0x02, 0xF6, 0x20, 0x01, 0x68, 0x56, 0x04, 0xF0, 0x05, 0x68, 0x52, 0x03, 0xF9, 0x10, 0x01, 0x88, 0x36, 0x0A, 0x0B, 0x9D, 0x95, 0x0D, 0x0E, 0xF8, 0x38, 0x01, 0xF1,
  0x3B, 0xFF, 0xF0, 0x00, 0x68, 0xF6, 0x08, 0xF4, 0x38, 0x01, 0xF5, 0x98, 0x6F, 0x0C, 0x0F, 0xF0, 0x00, 0x20, 0x01, 0x69, 0x58, 0x0A, 0xFF, 0x68, 0x87, 0x0D, 0xF0, 0x01, 0x38, 0x01, 0xF0, 0x09,
  0x68, 0x55, 0x05, 0xFD, 0x68, 0x89, 0x0D, 0xF0, 0x05, 0x38, 0x01, 0xF0, 0x07, 0x38, 0x55, 0xFA, 0x38, 0x53, 0xF0, 0x25, 0x38, 0x55, 0xFA, 0x68, 0x53, 0x01, 0xF0, 0x28, 0x3C, 0xAF, 0xF9, 0x3A,
  0x76, 0xF5, 0x38, 0x01, 0xF0, 0x1A, 0x6A, 0x53, 0x0C, 0xF7, 0x3C, 0xCB, 0xF1, 0x38, 0x01, 0xF0, 0x1E, 0x3D, 0x03, 0xF6, 0xA3, 0x04, 0x1B, 0x38, 0x01, 0xF0, 0x1A, 0x9E, 0x09, 0x00, 0xF9, 0x23,
  0xC0, 0x38, 0x01, 0xF0, 0x21, 0xA3, 0xBE, 0x1F, 0x98, 0x01, 0x02, 0x06, 0xF0, 0x2A, 0x38, 0x53, 0xF7, 0x9E, 0x00, 0x10, 0xF0, 0x2A, 0x6E, 0x04, 0xF9, 0x39, 0x4E, 0xF0, 0x27, 0xA4, 0xF0, 0x24,
  0x98, 0x01, 0x09, 0x06, 0xF0, 0x29, 0x3B, 0x43, 0xF9, 0x3A, 0x9C, 0xF0, 0x26, 0x38, 0xFA, 0xF8, 0x9E, 0x00, 0x08, 0xF0, 0x29, 0xA4, 0x15, 0x2E, 0xA2, 0x3D, 0x20, 0x38, 0x01, 0xF0, 0x1A, 0x38,
  0xA7, 0xF9, 0x3B, 0x44, 0xF0, 0x27, 0xA4, 0xC5, 0x1F, 0x3A, 0xF1, 0xF0, 0x26, 0x68, 0x01, 0x00, 0xFB, 0x39, 0xA3, 0xF0, 0x25, 0xA4, 0x92, 0x27, 0x68, 0x01, 0x06, 0xF0, 0x26, 0xA4, 0x32, 0x1A,
  0x68, 0x01, 0x06, 0xF0, 0x26, 0x38, 0x54, 0xFA, 0x3B, 0x46, 0xF0, 0x26, 0xA4, 0x61, 0x21, 0x3C, 0x41, 0xF0, 0x26, 0x38, 0x54, 0xF0, 0x8E, 0x6E, 0x09, 0xF0, 0x3B, 0x39, 0xF8, 0xF0, 0x3A, 0x6E,
  0x10, 0xFB, 0x38, 0x54, 0xF0, 0x26, 0x6E, 0x0C, 0xF0, 0x3B, 0x3C, 0x97, 0xFA, 0x3B, 0xF0, 0xF0, 0x27, 0x39, 0xA5, 0xFA, 0x38, 0x54, 0xF0, 0x26, 0x38, 0xFD, 0xFA, 0x6E, 0x07, 0xF0, 0x28, 0x6E,
  0x0B, 0xFA, 0xA2, 0x9A, 0x43, 0x38, 0x01, 0xF0, 0x1A, 0x38, 0xA9, 0xF9, 0xA2, 0x9F, 0x43, 0x38, 0x01, 0xF0, 0x1A, 0x3A, 0xA3, 0xF9, 0x3F, 0xD3, 0x09, 0xF0, 0x28, 0xA3, 0xE7, 0x1C, 0x3F, 0xDA,
  0x08, 0xF0, 0x28, 0xAB, 0xF2, 0x34, 0x0C, 0x3F, 0xE0, 0x07, 0xF0, 0x27, 0xA4, 0xF0, 0x29, 0x6E, 0x0D, 0xF0, 0x2A, 0x38, 0x55, 0xF8, 0x6E, 0x0C, 0xF0, 0x2B, 0x69, 0x54, 0x0C, 0xF7, 0xA4, 0x2A,
  0x35, 0x38, 0x01, 0xF0, 0x17, 0x39, 0xFE, 0xF4, 0x3F, 0x36, 0x08, 0xF0, 0x26, 0x20, 0x01, 0x6A, 0xFD, 0x0C, 0xF5, 0x3A, 0x4E, 0xF0, 0x28, 0x18, 0x01, 0x3C, 0xF8, 0xF2, 0xA2, 0xA7, 0x47, 0x38,
  0x01, 0xF0, 0x20, 0x3C, 0x4F, 0xF2, 0x3F, 0xD0, 0x0B, 0xF0, 0x29, 0x10, 0x01, 0x3C, 0xF7, 0xF3, 0x3A, 0xF7, 0xF0, 0x28, 0x08, 0x01, 0x3D, 0xF3, 0xF4, 0x3C, 0x48, 0xF0, 0x27, 0x10, 0x01, 0xA3,
  0x38, 0x26, 0x3A, 0xA3, 0xF0, 0x2B, 0x6E, 0x00, 0xF6, 0x38, 0xA9, 0xF0, 0x2A, 0x3D, 0x48, 0xF7, 0x38, 0xA9, 0xF0, 0x28, 0x3D, 0x47, 0xF8, 0x38, 0xA9, 0xF0, 0x28, 0x3D, 0xEF, 0xF9, 0x38, 0xA9,
  0xF0, 0x26, 0x3D, 0xEE, 0xF9, 0x3F, 0x90, 0x08, 0xF0, 0x27, 0x6E, 0x03, 0xFC, 0x6E, 0x10, 0xF0, 0x25, 0xA4, 0x4F, 0x54, 0x6B, 0xF6, 0x08, 0xF0, 0x24, 0xAC, 0xE9, 0x20, 0x05, 0x3C, 0x9F, 0xF0,
  0x25, 0xA4, 0xE5, 0x20, 0x10, 0x01, 0x3D, 0x48, 0xF0, 0x20, 0xA5, 0x37, 0x58, 0x69, 0xFC, 0x08, 0xF0, 0x20, 0xAB, 0x92, 0x1B, 0x05, 0x38, 0x01, 0x3F, 0x9A, 0x06, 0xF0, 0x1E, 0x39, 0xF4, 0xFB,
  0x28, 0x01, 0x3A, 0xFA, 0xF0, 0x1C, 0xA7, 0x3C, 0x65, 0x39, 0x53, 0xF0, 0x1A, 0xA7, 0x6A, 0x5D, 0x3D, 0x4A, 0xF0, 0x18, 0xA7, 0x1F, 0x5C, 0x6F, 0x2B, 0x0E, 0x08, 0xF0, 0x18, 0x68, 0x53, 0x05,
  0xF0, 0x09, 0x00, 0x55, 0x38, 0xAA, 0xF9, 0xE8, 0x0F, 0x0E, 0x09, 0x02, 0x02, 0x02, 0x0B, 0x07, 0x38, 0x1B, 0xF9, 0x6A, 0xED, 0x0A, 0xFF, 0x38, 0x01, 0xF5, 0xA3, 0xC8, 0x1B, 0xAA, 0x69, 0x5A,
  0x0F, 0x68, 0x57, 0x0B, 0xF7, 0x68, 0xA6, 0x10, 0xF0, 0x0B, 0x10, 0x01, 0x6A, 0xA8, 0x03, 0xF2, 0xAB, 0x16, 0x56, 0x05, 0x9B, 0xC4, 0x01, 0x0E, 0xF4, 0x6A, 0x45, 0x0C, 0xF0, 0x05, 0x38, 0x01,
  0xF5, 0x88, 0x39, 0x0B, 0x10, 0x98, 0x34, 0x0A, 0x04, 0xF0, 0x00, 0x8F, 0x62, 0x15, 0x03, 0x0E, 0x98, 0x52, 0x0F, 0x04, 0xF0, 0x16, 0x38, 0x01, 0xF0, 0xFF, 0xF0, 0x63, 0xE1, 0x00, 0x06, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
STATIC UINT8 AppleLogoBlackClut[ 51 ] = {
  0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x40, 0x40, 0x40, 0x50, 0x50, 0x50, 0x9F, 0x9F, 0x9F, 0xEF, 0xEF, 0xEF, 0xFF, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x10, 0x10, 0x10, 0x70, 0x70, 0x70, 0xCF, 0xCF,
  0xCF, 0x60, 0x60, 0x60, 0xDF, 0xDF, 0xDF, 0x30, 0x30, 0x30, 0x8F, 0x8F, 0x8F, 0xBF, 0xBF, 0xBF, 0xAF, 0xAF, 0xAF
};
*/

#define LZVN_0    0
#define LZVN_1    1
#define LZVN_2    2
#define LZVN_3    3
#define LZVN_4    4
#define LZVN_5    5
#define LZVN_6    6
#define LZVN_7    7
#define LZVN_8    8
#define LZVN_9    9
#define LZVN_10   10
#define LZVN_11   11

#define CASE_TABLE  127

//==============================================================================

UINTN
lzvn_decode (
  UINT8 *decompressedData,
  UINTN decompressedSize,
  UINT8 *compressedData,
  UINTN compressedSize
) {
  CONST UINT64 decompBuffer = (CONST UINT64)decompressedData;

  UINTN   length  = 0;                                                            // xor  %rax,%rax

  UINT64  compBuffer = (UINT64)compressedData,

          compBufferPointer = 0, // use p(ointer)?
          caseTableIndex = 0,
          byteCount = 0,
          currentLength = 0,                                                      // xor  %r12,%r12
          negativeOffset = 0,
          address = 0; // ((UINT64)compBuffer + compBufferPointer)

  UINT8   jmpTo = CASE_TABLE; // On the first run!

  // Example values:
  //
  // byteCount: 10, negativeOffset: 28957,  length: 42205762, currentLength: 42205772, compBufferPointer: 42176805
  // byteCount: 152,  negativeOffset: 28957,  length: 42205772, currentLength: 42205924, compBufferPointer: 42176815
  // byteCount: 10, negativeOffset: 7933, length: 42205924, currentLength: 42205934, compBufferPointer: 42197991
  // byteCount: 45, negativeOffset: 7933, length: 42205934, currentLength: 42205979, compBufferPointer: 42198001
  // byteCount: 9,  negativeOffset: 64,   length: 42205979, currentLength: 42205988, compBufferPointer: 42205915
  // byteCount: 10, negativeOffset: 8180, length: 42205988, currentLength: 42205998, compBufferPointer: 42197808
  // byteCount: 59, negativeOffset: 8180, length: 42205998, currentLength: 42206057, compBufferPointer: 42197818
  // byteCount: 10, negativeOffset: 359,  length: 42206057, currentLength: 42206067, compBufferPointer: 42205698
  // byteCount: 1,  negativeOffset: 359,  length: 42206067, currentLength: 42206068, compBufferPointer: 42205708
  // byteCount: 10, negativeOffset: 29021,  length: 42206068, currentLength: 42206078, compBufferPointer: 42177047
  //
  // length + byteCount = currentLength
  // currentLength - (negativeOffset + byteCount) = compBufferPointer
  // length - negativeOffset = compBufferPointer

  static short caseTable[ 256 ] = {
    1,  1,  1,  1,    1,  1,  2,  3,    1,  1,  1,  1,    1,  1,  4,  3,
    1,  1,  1,  1,    1,  1,  4,  3,    1,  1,  1,  1,    1,  1,  5,  3,
    1,  1,  1,  1,    1,  1,  5,  3,    1,  1,  1,  1,    1,  1,  5,  3,
    1,  1,  1,  1,    1,  1,  5,  3,    1,  1,  1,  1,    1,  1,  5,  3,
    1,  1,  1,  1,    1,  1,  0,  3,    1,  1,  1,  1,    1,  1,  0,  3,
    1,  1,  1,  1,    1,  1,  0,  3,    1,  1,  1,  1,    1,  1,  0,  3,
    1,  1,  1,  1,    1,  1,  0,  3,    1,  1,  1,  1,    1,  1,  0,  3,
    5,  5,  5,  5,    5,  5,  5,  5,    5,  5,  5,  5,    5,  5,  5,  5,
    1,  1,  1,  1,    1,  1,  0,  3,    1,  1,  1,  1,    1,  1,  0,  3,
    1,  1,  1,  1,    1,  1,  0,  3,    1,  1,  1,  1,    1,  1,  0,  3,
    6,  6,  6,  6,    6,  6,  6,  6,    6,  6,  6,  6,    6,  6,  6,  6,
    6,  6,  6,  6,    6,  6,  6,  6,    6,  6,  6,  6,    6,  6,  6,  6,
    1,  1,  1,  1,    1,  1,  0,  3,    1,  1,  1,  1,    1,  1,  0,  3,
    5,  5,  5,  5,    5,  5,  5,  5,    5,  5,  5,  5,    5,  5,  5,  5,
    7,  8,  8,  8,    8,  8,  8,  8,    8,  8,  8,  8,    8,  8,  8,  8,
    9, 10, 10, 10,   10, 10, 10, 10,   10, 10, 10, 10,   10, 10, 10, 10
  };

  decompressedSize -= 8;                                                          // sub  $0x8,%rsi

  if (decompressedSize < 8) {                                                     // jb Llzvn_exit
    return 0;
  }

  compressedSize = (compBuffer + compressedSize - 8);                             // lea  -0x8(%rdx,%rcx,1),%rcx

  if (compBuffer > compressedSize) {                                              // cmp  %rcx,%rdx
    return 0;                                                                     // ja Llzvn_exit
  }

  compBufferPointer = *(UINT64 *)compBuffer;                                      // mov  (%rdx),%r8
  caseTableIndex = (compBufferPointer & 255);                                     // movzbq (%rdx),%r9

  do {                                                                            // jmpq *(%rbx,%r9,8)
    switch (jmpTo) {                                                              // our jump table
      case CASE_TABLE: /******************************************************/
        switch (caseTable[(UINT8)caseTableIndex]) {
          case 0:
            DEBUG_STATE("caseTable[0]\n");
            caseTableIndex >>= 6;                                                 // shr  $0x6,%r9
            compBuffer = (compBuffer + caseTableIndex + 1);                       // lea  0x1(%rdx,%r9,1),%rdx

            if (compBuffer > compressedSize) {                                    // cmp  %rcx,%rdx
              return 0;                                                           // ja Llzvn_exit
            }

            byteCount = 56;                                                       // mov  $0x38,%r10
            byteCount &= compBufferPointer;                                       // and  %r8,%r10
            compBufferPointer >>= 8;                                              // shr  $0x8,%r8
            byteCount >>= 3;                                                      // shr  $0x3,%r10
            byteCount += 3;                                                       // add  $0x3,%r10

            jmpTo = LZVN_10;                                                      // jmp  Llzvn_l10
            break;

          case 1:
            DEBUG_STATE("caseTable[1]\n");
            caseTableIndex >>= 6;                                                 // shr  $0x6,%r9
            compBuffer = (compBuffer + caseTableIndex + 2);                       // lea  0x2(%rdx,%r9,1),%rdx

            if (compBuffer > compressedSize) {                                    // cmp  %rcx,%rdx
              return 0;                                                           // ja Llzvn_exit
            }

            negativeOffset = compBufferPointer;                                   // mov  %r8,%r12
            //negativeOffset = OSSwapInt64(negativeOffset);                       // bswap  %r12
            negativeOffset = SwapBytes64(negativeOffset);                         // bswap  %r12
            byteCount = negativeOffset;                                           // mov  %r12,%r10
            negativeOffset <<= 5;                                                 // shl  $0x5,%r12
            byteCount <<= 2;                                                      // shl  $0x2,%r10
            negativeOffset >>= 53;                                                // shr  $0x35,%r12
            byteCount >>= 61;                                                     // shr  $0x3d,%r10
            compBufferPointer >>= 16;                                             // shr  $0x10,%r8
            byteCount += 3;                                                       // add  $0x3,%r10

            jmpTo = LZVN_10;                                                      // jmp  Llzvn_l10
            break;

          case 2:
            DEBUG_STATE("caseTable[2]\n");
            return length;

          case 3:
            DEBUG_STATE("caseTable[3]\n");
            caseTableIndex >>= 6;                                                 // shr  $0x6,%r9
            compBuffer = (compBuffer + caseTableIndex + 3);                       // lea  0x3(%rdx,%r9,1),%rdx

            if (compBuffer > compressedSize) {                                    // cmp  %rcx,%rdx
              return 0;                                                           // ja Llzvn_exit
            }

            byteCount = 56;                                                       // mov  $0x38,%r10
            negativeOffset = 65535;                                               // mov  $0xffff,%r12
            byteCount &= compBufferPointer;                                       // and  %r8,%r10
            compBufferPointer >>= 8;                                              // shr  $0x8,%r8
            byteCount >>= 3;                                                      // shr  $0x3,%r10
            negativeOffset &= compBufferPointer;                                  // and  %r8,%r12
            compBufferPointer >>= 16;                                             // shr  $0x10,%r8
            byteCount += 3;                                                       // add  $0x3,%r10

            jmpTo = LZVN_10;                                                      // jmp  Llzvn_l10
            break;

          case 4:
            DEBUG_STATE("caseTable[4]\n");
            compBuffer++;                                                         // add  $0x1,%rdx

            if (compBuffer > compressedSize) {                                    // cmp  %rcx,%rdx
              return 0;                                                           // ja Llzvn_exit
            }

            compBufferPointer = *(UINT64 *)compBuffer;                            // mov  (%rdx),%r8
            caseTableIndex = (compBufferPointer & 255);                           // movzbq (%rdx),%r9

            jmpTo = CASE_TABLE;                                                   // continue;
            break;                                                                // jmpq *(%rbx,%r9,8)

          case 5:
            DEBUG_STATE("caseTable[5]\n");
            return 0;                                                             // Llzvn_table5;

          case 6:
            DEBUG_STATE("caseTable[6]\n");
            caseTableIndex >>= 3;                                                 // shr  $0x3,%r9
            caseTableIndex &= 3;                                                  // and  $0x3,%r9
            compBuffer = (compBuffer + caseTableIndex + 3);                       // lea  0x3(%rdx,%r9,1),%rdx

            if (compBuffer > compressedSize) {                                    // cmp  %rcx,%rdx
              return 0;                                                           // ja Llzvn_exit
            }

            byteCount = compBufferPointer;                                        // mov  %r8,%r10
            byteCount &= 775;                                                     // and  $0x307,%r10
            compBufferPointer >>= 10;                                             // shr  $0xa,%r8
            negativeOffset = (byteCount & 255);                                   // movzbq %r10b,%r12
            byteCount >>= 8;                                                      // shr  $0x8,%r10
            negativeOffset <<= 2;                                                 // shl  $0x2,%r12
            byteCount |= negativeOffset;                                          // or %r12,%r10
            negativeOffset = 16383;                                               // mov  $0x3fff,%r12
            byteCount += 3;                                                       // add  $0x3,%r10
            negativeOffset &= compBufferPointer;                                  // and  %r8,%r12
            compBufferPointer >>= 14;                                             // shr  $0xe,%r8

            jmpTo = LZVN_10;                                                      // jmp  Llzvn_l10
            break;

          case 7:
            DEBUG_STATE("caseTable[7]\n");
            compBufferPointer >>= 8;                                              // shr  $0x8,%r8
            compBufferPointer &= 255;                                             // and  $0xff,%r8
            compBufferPointer += 16;                                              // add  $0x10,%r8
            compBuffer = (compBuffer + compBufferPointer + 2);                    // lea  0x2(%rdx,%r8,1),%rdx

            jmpTo = LZVN_0;                                                       // jmp  Llzvn_l0
            break;

          case 8:
            DEBUG_STATE("caseTable[8]\n");
            compBufferPointer &= 15;                                              // and  $0xf,%r8
            compBuffer = (compBuffer + compBufferPointer + 1);                    // lea  0x1(%rdx,%r8,1),%rdx

            jmpTo = LZVN_0;                                                       // jmp  Llzvn_l0
            break;

          case 9:
            DEBUG_STATE("caseTable[9]\n");
            compBuffer += 2;                                                      // add  $0x2,%rdx

            if (compBuffer > compressedSize) {                                    // cmp  %rcx,%rdx
              return 0;                                                           // ja Llzvn_exit
            }

            // Up most significant byte (count) by 16 (0x10/16 - 0x10f/271).
            byteCount = compBufferPointer;                                        // mov  %r8,%r10
            byteCount >>= 8;                                                      // shr  $0x8,%r10
            byteCount &= 255;                                                     // and  $0xff,%r10
            byteCount += 16;                                                      // add  $0x10,%r10

            jmpTo = LZVN_11;                                                      // jmp  Llzvn_l11
            break;

          case 10:
            DEBUG_STATE("caseTable[10]\n");
            compBuffer++;                                                         // add  $0x1,%rdx

            if (compBuffer > compressedSize) {                                    // cmp  %rcx,%rdx
              return 0;                                                           // ja Llzvn_exit
            }

            byteCount = compBufferPointer;                                        // mov  %r8,%r10
            byteCount &= 15;                                                      // and  $0xf,%r10

            jmpTo = LZVN_11;                                                      // jmp  Llzvn_l11
            break;

          default:
            DEBUG_STATE("default() caseTableIndex[%d]\n", (UINT8)caseTableIndex);
            break;
        } // switch (caseTable[caseTableIndex])

        break;

      case LZVN_0: /**********************************************************/
        DEBUG_STATE("jmpTable(0)\n");

        if (compBuffer > compressedSize) {                                          // cmp  %rcx,%rdx
          return 0;                                                                 // ja Llzvn_exit
        }

        currentLength = (length + compBufferPointer);                               // lea  (%rax,%r8,1),%r11
        //compBufferPointer = -compBufferPointer;                                     // neg  %r8
        compBufferPointer = compBufferPointer * -1;                                 // neg  %r8

        if (currentLength > decompressedSize) {                                     // cmp  %rsi,%r11
          jmpTo = LZVN_2;                                                           // ja Llzvn_l2
          break;
        }

        currentLength = (decompBuffer + currentLength);                             // lea  (%rdi,%r11,1),%r11

      case LZVN_1: /**********************************************************/
        do {                                                                        // Llzvn_l1:
          DEBUG_STATE("jmpTable(1)\n");

          //caseTableIndex = *(UINT64 *)((UINT64)compBuffer + compBufferPointer);

          address = (compBuffer + compBufferPointer);                               // mov  (%rdx,%r8,1),%r9
          caseTableIndex = *(UINT64 *)address;

          //*(UINT64 *)((UINT64)currentLength + compBufferPointer) = caseTableIndex;
          // or:
          //CopyMem((VOID *)currentLength + compBufferPointer, &caseTableIndex, 8);
          // or:
          address = (currentLength + compBufferPointer);                            // mov  %r9,(%r11,%r8,1)
          *(UINT64 *)address = caseTableIndex;
          compBufferPointer += 8;                                                   // add  $0x8,%r8

        } while ((EFI_MAX_ADDRESS - (compBufferPointer - 8)) >= 8);                 // jae  Llzvn_l1

        length = currentLength;                                                     // mov  %r11,%rax
        length -= decompBuffer;                                                     // sub  %rdi,%rax

        compBufferPointer = *(UINT64 *)compBuffer;                                  // mov  (%rdx),%r8
        caseTableIndex = (compBufferPointer & 255);                                 // movzbq (%rdx),%r9

        jmpTo = CASE_TABLE;
        break;                                                                      // jmpq *(%rbx,%r9,8)

      case LZVN_2: /**********************************************************/
        DEBUG_STATE("jmpTable(2)\n");
        currentLength = (decompressedSize + 8);                                     // lea  0x8(%rsi),%r11

      case LZVN_3: /***********************************************************/
        do {                                                                        // Llzvn_l3: (block copy of bytes)
          DEBUG_STATE("jmpTable(3)\n");

          address = (compBuffer + compBufferPointer);                               // movzbq (%rdx,%r8,1),%r9
          caseTableIndex = (*((UINT64 *)address) & 255);
          CopyMem((UINT8 *)decompBuffer + length, &caseTableIndex, 1);
          length++;                                                                 // add  $0x1,%rax

          if (currentLength == length) {                                            // cmp  %rax,%r11
            return length;                                                          // je Llzvn_exit2
          }

          compBufferPointer++;                                                      // add  $0x1,%r8
        } while ((INT64)compBufferPointer != 0);                                    // jne  Llzvn_l3

        compBufferPointer = *(UINT64 *)compBuffer;                                  // mov  (%rdx),%r8
        caseTableIndex = (compBufferPointer & 255);                                 // movzbq (%rdx),%r9

        jmpTo = CASE_TABLE;
        break;                                                                      // jmpq *(%rbx,%r9,8)

      case LZVN_4: /**********************************************************/
        DEBUG_STATE("jmpTable(4)\n");
        currentLength = (decompressedSize + 8);                                     // lea  0x8(%rsi),%r11

      case LZVN_9: /**********************************************************/
        do {                                                                        // Llzvn_l9: (block copy of bytes)
          DEBUG_STATE("jmpTable(9)\n");

          address = (decompBuffer + compBufferPointer);                             // movzbq (%rdi,%r8,1),%r9
          caseTableIndex = (*((UINT8 *)address) & 255);

          compBufferPointer++;                                                      // add  $0x1,%r8
          CopyMem((UINT8 *)decompBuffer + length, &caseTableIndex, 1);              // mov  %r9,(%rdi,%rax,1)
          length++;                                                                 // add  $0x1,%rax

          if (length == currentLength) {                                            // cmp  %rax,%r11
            return length;                                                          // je Llzvn_exit2
          }

          byteCount--;                                                              // sub  $0x1,%r10
        } while (byteCount);                                                        // jne  Llzvn_l9

        compBufferPointer = *(UINT64 *)compBuffer;                                  // mov  (%rdx),%r8
        caseTableIndex = (compBufferPointer & 255);                                 // movzbq (%rdx),%r9

        jmpTo = CASE_TABLE;
        break;                                                                      // jmpq *(%rbx,%r9,8)

      case LZVN_5: /**********************************************************/
        do {                                                                        // Llzvn_l5: (block copy of qwords)
          DEBUG_STATE("jmpTable(5)\n");

          address = (decompBuffer + compBufferPointer);                             // mov  (%rdi,%r8,1),%r9
          caseTableIndex = *((UINT64 *)address);

          compBufferPointer += 8;                                                   // add  $0x8,%r8
          CopyMem((UINT8 *)decompBuffer + length, &caseTableIndex, 8);               // mov  %r9,(%rdi,%rax,1)
          length += 8;                                                              // add  $0x8,%rax
          byteCount -= 8;                                                           // sub  $0x8,%r10
        } while ((byteCount + 8) > 8);                                              // ja Llzvn_l5

        length += byteCount;                                                        // add  %r10,%rax
        compBufferPointer = *(UINT64 *)compBuffer;                                  // mov  (%rdx),%r8
        caseTableIndex = (compBufferPointer & 255);                                 // movzbq (%rdx),%r9

        jmpTo = CASE_TABLE;
        break;                                                                      // jmpq *(%rbx,%r9,8)

      case LZVN_10: /*********************************************************/
        DEBUG_STATE("jmpTable(10)\n");

        currentLength = (length + caseTableIndex);                                  // lea  (%rax,%r9,1),%r11
        currentLength += byteCount;                                                 // add  %r10,%r11

        if (currentLength < decompressedSize) {                                     // cmp  %rsi,%r11 (block_end: jae Llzvn_l8)
          CopyMem((UINT8 *)decompBuffer + length, &compBufferPointer, 8);           // mov  %r8,(%rdi,%rax,1)
          length += caseTableIndex;                                                 // add  %r9,%rax
          compBufferPointer = length;                                               // mov  %rax,%r8

          if (compBufferPointer < negativeOffset) {                                 // jb Llzvn_exit
            return 0;
          }

          compBufferPointer -= negativeOffset;                                      // sub  %r12,%r8

          if (negativeOffset < 8) {                                                 // cmp  $0x8,%r12
            jmpTo = LZVN_4;                                                         // jb Llzvn_l4
            break;
          }

          jmpTo = LZVN_5;                                                           // jmpq *(%rbx,%r9,8)
          break;
        }

      case LZVN_8: /**********************************************************/
        DEBUG_STATE("jmpTable(8)\n");

        if (caseTableIndex == 0) {                                                  // test %r9,%r9
          jmpTo = LZVN_7;                                                           // jmpq *(%rbx,%r9,8)
          break;
        }

        currentLength = (decompressedSize + 8);                                     // lea  0x8(%rsi),%r11
        //

      case LZVN_6: /**********************************************************/
        do {
          DEBUG_STATE("jmpTable(6)\n");

          CopyMem((UINT8 *)decompBuffer + length, &compBufferPointer, 1);           // mov  %r8b,(%rdi,%rax,1)
          length++;                                                                 // add  $0x1,%rax

          if (length == currentLength) {                                            // cmp  %rax,%r11
            return length;                                                          // je Llzvn_exit2
          }

          compBufferPointer >>= 8;                                                  // shr  $0x8,%r8
          caseTableIndex--;                                                         // sub  $0x1,%r9
        } while (caseTableIndex != 1);                                              // jne  Llzvn_l6

      case LZVN_7: /**********************************************************/
        DEBUG_STATE("jmpTable(7)\n");

        compBufferPointer = length;                                                 // mov  %rax,%r8
        compBufferPointer -= negativeOffset;                                        // sub  %r12,%r8

        if (compBufferPointer < negativeOffset) {                                   // jb Llzvn_exit
          return 0;
        }

        jmpTo = LZVN_4;
        break;                                                                      // jmpq *(%rbx,%r9,8)

      case LZVN_11: /*********************************************************/
        DEBUG_STATE("jmpTable(11)\n");

        compBufferPointer = length;                                                 // mov  %rax,%r8
        compBufferPointer -= negativeOffset;                                        // sub  %r12,%r8
        currentLength = (length + byteCount);                                       // lea  (%rax,%r10,1),%r11

        if (currentLength < decompressedSize) {                                     // cmp  %rsi,%r11
          if (negativeOffset >= 8) {                                                // cmp  $0x8,%r12
            jmpTo = LZVN_5;                                                         // jae  Llzvn_l5
            break;
          }
        }

        jmpTo = LZVN_4;                                                             // jmp  Llzvn_l4
        break;
    }                                                                               // switch (jmpq)
  } while (1);

  return 0;
}

#if 0
STATIC
EFI_STATUS
CspConvertImage (
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL** imageBuffer,
  UINT8 CONST* imageData,
  UINTN width,
  UINTN height,
  //UINTN frameCount,
  UINT8 CONST* lookupTable
) {
  UINTN                           dataLength = width * height/* * frameCount*/,
                                  imageLength = dataLength * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *theImage = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)AllocateZeroPool(imageLength);

  if (!theImage) {
    return EFI_OUT_OF_RESOURCES;
  }

  *imageBuffer = theImage;

  for (UINTN i = 0; i < dataLength; i++, theImage++, imageData++) {
    UINTN index = *imageData * 3;

    theImage->Red = lookupTable[index + 0];
    theImage->Green = lookupTable[index + 1];
    theImage->Blue = lookupTable[index + 2];
  }

  return EFI_SUCCESS;
}
#endif

VOID
hehe () {
  UINTN compressedSize = sizeof(AppleLogoBlackPacked);
  UINT8 *logoData = (UINT8 *)AppleLogoBlackPacked;
  UINTN logoSize = (APPLE_LOGO_WIDTH * APPLE_LOGO_HEIGHT);
  UINT8 *decompressedData = AllocatePool(logoSize);

  if (decompressedData) {
    if (lzvn_decode(decompressedData, logoSize, logoData, compressedSize) == logoSize) {
      //UINT8 *bootImageData = NULL;
      //EFI_GRAPHICS_OUTPUT_BLT_PIXEL* logoImage;

      //convertImage(APPLE_LOGO_WIDTH, APPLE_LOGO_HEIGHT, decompressedData, &bootImageData);
      //drawDataRectangle(APPLE_LOGO_X, APPLE_LOGO_Y, APPLE_LOGO_WIDTH, APPLE_LOGO_HEIGHT, bootImageData);
      //CspConvertImage(&logoImage, decompressedData, APPLE_LOGO_WIDTH, APPLE_LOGO_HEIGHT, /*1,*/ (UINT8 *)AppleLogoBlackClut);
      if (
        decompressedData[1] == 'P' &&
        decompressedData[2] == 'N' &&
        decompressedData[3] == 'G'
      ) {
        DBG ("lzvn_decode: Correct PNG Header\n");
      } else {
        DBG ("lzvn_decode: Incorrect PNG Header\n");
        DBG ("lzvn_decode: %02x - %02x - %02x\n", decompressedData[1], decompressedData[2], decompressedData[3]);
      }
    } else {
      DBG ("lzvn_decode: failed\n");
    }

    FreePool(decompressedData);
  }
}
