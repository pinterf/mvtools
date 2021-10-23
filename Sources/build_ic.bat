rem icl /FAs /D__AVX2__ /D__AVX512F__ /QxHost /Qm64 /Qopenmp /O3 /Ot /Oi /Qunroll /tune:skylake /arch:COFFEELAKE /QaxCOFFEELAKE /Qopt-mem-layout-trans:3 /Qopt-streaming-stores:always /Qunroll-aggressive /Qopt-prefetch:5 /Qopt-prefetch-issue-excl-hint /Qalign-loops:32 /Qoverride-limits -EHsc /LD src/JincResize.cpp src/resize_plane_avx2.cpp src/resize_plane_avx512.cpp src/resize_plane_sse41.cpp src/KernelRow_avx2.cpp src/KernelRow_sse41.cpp src/KernelRow_avx512.cpp src/test.cpp

icl /IC:\cpp\mvtools-test_new_Esa_sp2\Sources\include\ /IC:\cpp\mvtools-test_new_Esa_sp2\Sources /FAs /D__AVX2__ /tune:skylake /Qm64 /O3 /Ot /Oi /Qunroll /Qunroll-aggressive /Qalign-loops:32 -EHsc /c /MD /Z7 PlaneOfBlocks_avx2.cpp

lib /OUT:PlaneofBlocks_avx2.lib PlaneofBlocks_avx2.obj

