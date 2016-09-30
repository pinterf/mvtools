#include "SADFunctions_avx.h"
//#include "overlap.h"
#include <map>
#include <tuple>

SADFunction* get_sad_avx_C_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, SADFunction*> func_sad;
    using std::make_tuple;

    func_sad[make_tuple(32, 32, 1, NO_SIMD)] = Sad_AVX_C<32, 32,uint8_t>;
    func_sad[make_tuple(32, 16, 1, NO_SIMD)] = Sad_AVX_C<32, 16,uint8_t>;
    func_sad[make_tuple(32, 8 , 1, NO_SIMD)] = Sad_AVX_C<32, 8,uint8_t>;
    func_sad[make_tuple(16, 32, 1, NO_SIMD)] = Sad_AVX_C<16, 32,uint8_t>;
    func_sad[make_tuple(16, 16, 1, NO_SIMD)] = Sad_AVX_C<16, 16,uint8_t>;
    func_sad[make_tuple(16, 8 , 1, NO_SIMD)] = Sad_AVX_C<16, 8,uint8_t>;
    func_sad[make_tuple(16, 4 , 1, NO_SIMD)] = Sad_AVX_C<16, 4,uint8_t>;
    func_sad[make_tuple(16, 2 , 1, NO_SIMD)] = Sad_AVX_C<16, 2,uint8_t>;
    func_sad[make_tuple(16, 1 , 1, NO_SIMD)] = Sad_AVX_C<16, 1,uint8_t>;
    func_sad[make_tuple(8 , 16, 1, NO_SIMD)] = Sad_AVX_C<8 , 16,uint8_t>;
    func_sad[make_tuple(8 , 8 , 1, NO_SIMD)] = Sad_AVX_C<8 , 8,uint8_t>;
    func_sad[make_tuple(8 , 4 , 1, NO_SIMD)] = Sad_AVX_C<8 , 4,uint8_t>;
    func_sad[make_tuple(8 , 2 , 1, NO_SIMD)] = Sad_AVX_C<8 , 2,uint8_t>;
    func_sad[make_tuple(8 , 1 , 1, NO_SIMD)] = Sad_AVX_C<8 , 1,uint8_t>;
    func_sad[make_tuple(4 , 8 , 1, NO_SIMD)] = Sad_AVX_C<4 , 8,uint8_t>;
    func_sad[make_tuple(4 , 4 , 1, NO_SIMD)] = Sad_AVX_C<4 , 4,uint8_t>;
    func_sad[make_tuple(4 , 2 , 1, NO_SIMD)] = Sad_AVX_C<4 , 2,uint8_t>;
    func_sad[make_tuple(4 , 1 , 1, NO_SIMD)] = Sad_AVX_C<4 , 1,uint8_t>;
    func_sad[make_tuple(2 , 4 , 1, NO_SIMD)] = Sad_AVX_C<2 , 4,uint8_t>;
    func_sad[make_tuple(2 , 2 , 1, NO_SIMD)] = Sad_AVX_C<2 , 2,uint8_t>;
    func_sad[make_tuple(2 , 1 , 1, NO_SIMD)] = Sad_AVX_C<2 , 1,uint8_t>;

    func_sad[make_tuple(32, 32, 2, NO_SIMD)] = Sad_AVX_C<32, 32,uint16_t>;
    func_sad[make_tuple(32, 16, 2, NO_SIMD)] = Sad_AVX_C<32, 16,uint16_t>;
    func_sad[make_tuple(32, 8 , 2, NO_SIMD)] = Sad_AVX_C<32, 8,uint16_t>;
    func_sad[make_tuple(16, 32, 2, NO_SIMD)] = Sad_AVX_C<16, 32,uint16_t>;
    func_sad[make_tuple(16, 16, 2, NO_SIMD)] = Sad_AVX_C<16, 16,uint16_t>;
    func_sad[make_tuple(16, 8 , 2, NO_SIMD)] = Sad_AVX_C<16, 8,uint16_t>;
    func_sad[make_tuple(16, 4 , 2, NO_SIMD)] = Sad_AVX_C<16, 4,uint16_t>;
    func_sad[make_tuple(16, 2 , 2, NO_SIMD)] = Sad_AVX_C<16, 2,uint16_t>;
    func_sad[make_tuple(16, 1 , 2, NO_SIMD)] = Sad_AVX_C<16, 1,uint16_t>;
    func_sad[make_tuple(8 , 16, 2, NO_SIMD)] = Sad_AVX_C<8 , 16,uint16_t>;
    func_sad[make_tuple(8 , 8 , 2, NO_SIMD)] = Sad_AVX_C<8 , 8,uint16_t>;
    func_sad[make_tuple(8 , 4 , 2, NO_SIMD)] = Sad_AVX_C<8 , 4,uint16_t>;
    func_sad[make_tuple(8 , 2 , 2, NO_SIMD)] = Sad_AVX_C<8 , 2,uint16_t>;
    func_sad[make_tuple(8 , 1 , 2, NO_SIMD)] = Sad_AVX_C<8 , 1,uint16_t>;
    func_sad[make_tuple(4 , 8 , 2, NO_SIMD)] = Sad_AVX_C<4 , 8,uint16_t>;
    func_sad[make_tuple(4 , 4 , 2, NO_SIMD)] = Sad_AVX_C<4 , 4,uint16_t>;
    func_sad[make_tuple(4 , 2 , 2, NO_SIMD)] = Sad_AVX_C<4 , 2,uint16_t>;
    func_sad[make_tuple(4 , 1 , 2, NO_SIMD)] = Sad_AVX_C<4 , 1,uint16_t>;
    func_sad[make_tuple(2 , 4 , 2, NO_SIMD)] = Sad_AVX_C<2 , 4,uint16_t>;
    func_sad[make_tuple(2 , 2 , 2, NO_SIMD)] = Sad_AVX_C<2 , 2,uint16_t>;
    func_sad[make_tuple(2 , 1 , 2, NO_SIMD)] = Sad_AVX_C<2 , 1,uint16_t>;

    SADFunction *result = func_sad[make_tuple(BlockX, BlockY, pixelsize, arch)];
    return result;
}

