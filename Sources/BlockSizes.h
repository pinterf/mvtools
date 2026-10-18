#ifndef __BLOCKSIZES_H__
#define __BLOCKSIZES_H__

// The block sizes of the C function tables: copy, overlaps (also lsb), SAD, MDegrain1-6, MDegrainN, luma.
// One list for all of them, so that no table can miss a size (e.g. luma lacked 12x3 and 6x24: crash in MAnalyse).
// SIMD versions are registered separately, only for some sizes, with C as fallback.
// Usage: MVTOOLS_BLOCKSIZES(MAKE_XXX_FN) where MAKE_XXX_FN(x, y) registers one size.
//
// It contains the luma block sizes allowed in MAnalyse and MRecalculate, and their chroma sizes
// (luma block size divided by the subsampling) which are not luma block sizes themselves:
// 64x24, 64x8, 48x32, 48x6, 32x48, 32x12, 32x4, 24x64, 24x16, 24x3, 16x48, 16x24, 16x6, 16x1, 12x64, 12x32, 12x8, 8x64, 8x24, 8x12,
// 8x6, 6x48, 6x32, 6x16, 6x8, 4x64, 4x32, 4x16, 4x12, 4x3, 4x1, 3x48, 3x24, 3x16, 3x12, 3x4, 2x32, 2x16, 2x8, 2x1, 1x8, 1x4, 1x2, 1x1.
// E.g. 16x12 in 4:2:0 -> 8x6, 32x24 in 4:2:2 -> 16x24, 4x4 in 4:1:1 -> 1x4, in 4:1:0 -> 1x1, 64x48 in 4:4:0 -> 64x24.
#define MVTOOLS_BLOCKSIZES(MAKE) \
  MAKE(64, 64) MAKE(64, 48) MAKE(64, 32) MAKE(64, 24) MAKE(64, 16) MAKE(64, 8) \
  MAKE(48, 64) MAKE(48, 48) MAKE(48, 32) MAKE(48, 24) MAKE(48, 12) MAKE(48, 6) \
  MAKE(32, 64) MAKE(32, 48) MAKE(32, 32) MAKE(32, 24) MAKE(32, 16) MAKE(32, 12) MAKE(32, 8) MAKE(32, 4) \
  MAKE(24, 64) MAKE(24, 48) MAKE(24, 32) MAKE(24, 24) MAKE(24, 16) MAKE(24, 12) MAKE(24, 6) MAKE(24, 3) \
  MAKE(16, 64) MAKE(16, 48) MAKE(16, 32) MAKE(16, 24) MAKE(16, 16) MAKE(16, 12) MAKE(16, 8) MAKE(16, 6) MAKE(16, 4) MAKE(16, 2) MAKE(16, 1) \
  MAKE(12, 64) MAKE(12, 48) MAKE(12, 32) MAKE(12, 24) MAKE(12, 16) MAKE(12, 12) MAKE(12, 8) MAKE(12, 6) MAKE(12, 3) \
  MAKE(8, 64) MAKE(8, 32) MAKE(8, 24) MAKE(8, 16) MAKE(8, 12) MAKE(8, 8) MAKE(8, 6) MAKE(8, 4) MAKE(8, 2) MAKE(8, 1) \
  MAKE(6, 48) MAKE(6, 32) MAKE(6, 24) MAKE(6, 16) MAKE(6, 12) MAKE(6, 8) MAKE(6, 6) MAKE(6, 3) \
  MAKE(4, 64) MAKE(4, 32) MAKE(4, 16) MAKE(4, 12) MAKE(4, 8) MAKE(4, 4) MAKE(4, 3) MAKE(4, 2) MAKE(4, 1) \
  MAKE(3, 48) MAKE(3, 24) MAKE(3, 16) MAKE(3, 12) MAKE(3, 6) MAKE(3, 4) MAKE(3, 3) \
  MAKE(2, 32) MAKE(2, 16) MAKE(2, 8) MAKE(2, 4) MAKE(2, 2) MAKE(2, 1) \
  MAKE(1, 8) MAKE(1, 4) MAKE(1, 2) MAKE(1, 1)

#endif
