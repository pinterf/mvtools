// Lite version of fftw header
// Copyright(c)2005, Fizick
// fftwf typedefs added in v.1.2 for delayed loading
// 
#ifndef __FFTWLITE_H__
#define __FFTWLITE_H__
typedef float fftwf_complex[2];
typedef struct fftwf_plan_s  *fftwf_plan;
typedef fftwf_complex* (*fftwf_malloc_proc)(size_t n); 
typedef void (*fftwf_free_proc) (void *ppp);
typedef fftwf_plan (*fftwf_plan_dft_r2c_2d_proc) (int winy, int winx, float *realcorrel, fftwf_complex *correl, int flags);
typedef fftwf_plan (*fftwf_plan_dft_c2r_2d_proc) (int winy, int winx, fftwf_complex *correl, float *realcorrel, int flags);
typedef void (*fftwf_destroy_plan_proc) (fftwf_plan);
typedef void (*fftwf_execute_dft_r2c_proc) (fftwf_plan, float *realdata, fftwf_complex *fftsrc);
typedef void (*fftwf_execute_dft_c2r_proc) (fftwf_plan, fftwf_complex *fftsrc, float *realdata);
#define FFTW_ESTIMATE (1U << 6)
#define FFTW_REDFT01 4 /* idct */
#define FFTW_REDFT10 5 /* dct */
typedef fftwf_plan (*fftwf_plan_r2r_2d_proc) (int nx, int ny, float *in, float *out, int kindx, int kindy, unsigned flags);
typedef void (*fftwf_execute_r2r_proc) (const fftwf_plan plan, float *in, float *out);

#endif