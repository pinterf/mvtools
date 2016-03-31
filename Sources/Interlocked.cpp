#define conc_Interlocked_CODEBODY
#include "conc/Interlocked.h"

#pragma optimize( "", off ) // Make function body
#pragma warning (push)
#pragma warning (disable : 4700)

void dummy(void)
{
	volatile int32_t int32_t_a = 0, int32_t_b = 0, int32_t_c= 0;
	conc::Interlocked::swap(int32_t_a, int32_t_b);
	conc::Interlocked::cas(int32_t_a, int32_t_b, int32_t_c);

	volatile int64_t int64_t_a = 0, int64_t_b = 0, int64_t_c = 0;
	conc::Interlocked::swap(int64_t_a, int64_t_b);
	conc::Interlocked::cas(int64_t_a, int64_t_b, int64_t_c);

#if defined (conc_HAS_CAS_128)
	conc::Interlocked::Data128 Data128_a, Data128_b, Data128_c, Data128_d;
	conc::Interlocked::swap(Data128_a, Data128_b, Data128_c);
	conc::Interlocked::cas(Data128_a, Data128_b, Data128_c, Data128_d);

	Data128_a == Data128_b;
	Data128_a != Data128_b;
#endif

	void *voidp_a = 0, *voidp_b = 0, *voidp_c = 0;
	conc::Interlocked::swap(voidp_a, voidp_b);
	conc::Interlocked::cas(voidp_a, voidp_b, voidp_c);
}

#pragma warning (pop)
