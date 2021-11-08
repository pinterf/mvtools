// Lite version of fftw header
// fftwf typedefs added in v.1.2 for delayed loading
// additions, non-windows mix from neo_FFT3D
// 
#ifndef __FFTWLITE_H__
#define __FFTWLITE_H__

#include <stdexcept>

#ifdef _WIN32
#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HMODULE lib_t;
typedef FARPROC func_t;
#else
#include <dlfcn.h>
typedef void* lib_t;
typedef void* func_t;
#define __stdcall
#endif

typedef float fftwf_complex[2];
typedef struct fftwf_plan_s  *fftwf_plan;
typedef fftwf_complex* (*fftwf_malloc_proc)(size_t n); 
typedef void (*fftwf_free_proc) (void *ppp);
typedef fftwf_plan (*fftwf_plan_dft_r2c_2d_proc) (int winy, int winx, float *realcorrel, fftwf_complex *correl, int flags);
typedef fftwf_plan (*fftwf_plan_dft_c2r_2d_proc) (int winy, int winx, fftwf_complex *correl, float *realcorrel, int flags);
typedef fftwf_plan(*fftwf_plan_many_dft_r2c_proc) (int rank, const int* n, int howmany, float* in, const int* inembed, int istride, int idist, fftwf_complex* out, const int* onembed, int ostride, int odist, unsigned flags);
typedef fftwf_plan(*fftwf_plan_many_dft_c2r_proc) (int rank, const int* n, int howmany, fftwf_complex* out, const int* inembed, int istride, int idist, float* in, const int* onembed, int ostride, int odist, unsigned flags);
typedef fftwf_plan(*fftwf_plan_r2r_2d_proc) (int nx, int ny, float* in, float* out, int kindx, int kindy, unsigned flags);
typedef void (*fftwf_destroy_plan_proc) (fftwf_plan);
typedef void (*fftwf_execute_dft_r2c_proc) (fftwf_plan, float *realdata, fftwf_complex *fftsrc);
typedef void (*fftwf_execute_dft_c2r_proc) (fftwf_plan, fftwf_complex *fftsrc, float *realdata);
typedef void(*fftwf_execute_r2r_proc) (const fftwf_plan plan, float* in, float* out);
#define FFTW_MEASURE (0U)
#define FFTW_ESTIMATE (1U << 6)
#define FFTW_REDFT01 4 /* idct */
#define FFTW_REDFT10 5 /* dct */
typedef int (*fftwf_init_threads_proc) ();
typedef void (*fftwf_plan_with_nthreads_proc)(int nthreads);

#define LOAD_FFT_FUNC(name) do {name = reinterpret_cast<name ## _proc>((void*)fftw3_address(#name)); if (name == nullptr) throw "Library function is missing: " #name; } while(0)
#define LOAD_FFT_FUNC_OPT(name) do {name = reinterpret_cast<name ## _proc>((void*)fftw3_address(#name)); } while(0)

struct FFTFunctionPointers {
  bool preloaded{ false };
  lib_t library{ nullptr };

  fftwf_malloc_proc fftwf_malloc{ nullptr };
  fftwf_free_proc fftwf_free{ nullptr };
  fftwf_plan_many_dft_r2c_proc fftwf_plan_many_dft_r2c{ nullptr };
  fftwf_plan_many_dft_c2r_proc fftwf_plan_many_dft_c2r{ nullptr };
  fftwf_plan_r2r_2d_proc fftwf_plan_r2r_2d{ nullptr };
  fftwf_destroy_plan_proc fftwf_destroy_plan{ nullptr };
  fftwf_execute_dft_r2c_proc fftwf_execute_dft_r2c{ nullptr };
  fftwf_execute_dft_c2r_proc fftwf_execute_dft_c2r{ nullptr };
  fftwf_execute_r2r_proc fftwf_execute_r2r{ nullptr };
  fftwf_init_threads_proc fftwf_init_threads{ nullptr };
  fftwf_plan_with_nthreads_proc fftwf_plan_with_nthreads{ nullptr };
  fftwf_plan_dft_r2c_2d_proc fftwf_plan_dft_r2c_2d{ nullptr };
  fftwf_plan_dft_c2r_2d_proc fftwf_plan_dft_c2r_2d{ nullptr };

#ifdef _WIN32
  void fftw3_open() {
    library = LoadLibraryW(L"libfftw3f-3");
    if (library == nullptr)
      library = LoadLibraryW(L"fftw3");
    if (library == nullptr)
      throw std::runtime_error("libfftw3f-3.dll or fftw3.dll not found. Please put in PATH or use LoadDll() plugin");
  }
  void fftw3_close() { FreeLibrary(library); library = nullptr; }
  func_t fftw3_address(LPCSTR func) { return GetProcAddress(library, func); }
#else
  void fftw3_open() {
    library = dlopen("libfftw3f_threads.so.3", RTLD_NOW);
    if (library == nullptr)
      throw std::runtime_error("libfftw3f_threads.so.3 not found. Please install libfftw3-single3 (deb) or fftw-devel (rpm) package");
    // sudo apt-get update
    // sudo apt-get install libfftw3-dev
    // sudo apt install libfftw3-3
  }
  void fftw3_close() { dlclose(library); library = nullptr; }
  func_t fftw3_address(const char* func) { return dlsym(library, func); }
#endif
  void load() {
    library = nullptr;
    fftw3_open();
    if (library != nullptr) {
      LOAD_FFT_FUNC(fftwf_malloc);
      LOAD_FFT_FUNC(fftwf_free);
      LOAD_FFT_FUNC(fftwf_plan_many_dft_r2c);
      LOAD_FFT_FUNC(fftwf_plan_many_dft_c2r);
      LOAD_FFT_FUNC(fftwf_plan_r2r_2d);
      LOAD_FFT_FUNC(fftwf_destroy_plan);
      LOAD_FFT_FUNC(fftwf_execute_dft_r2c);
      LOAD_FFT_FUNC(fftwf_execute_dft_c2r);
      LOAD_FFT_FUNC(fftwf_execute_r2r);
      LOAD_FFT_FUNC_OPT(fftwf_init_threads);
      LOAD_FFT_FUNC_OPT(fftwf_plan_with_nthreads);
      LOAD_FFT_FUNC_OPT(fftwf_plan_dft_r2c_2d);
      LOAD_FFT_FUNC_OPT(fftwf_plan_dft_c2r_2d);
    }
  }

  void freelib() {
    if (library != nullptr) {
      fftw3_close();
    }
  }

  bool has_threading() {
    return library && fftwf_init_threads && fftwf_plan_with_nthreads;
  }
};

#undef LOAD_FFT_FUNC
#endif
