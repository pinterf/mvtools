/*****************************************************************************

        AvstpFinder.cpp
        Author: Laurent de Soras, 2012

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (_MSC_VER)
	#pragma warning (1 : 4130 4223 4705 4706)
	#pragma warning (4 : 4355 4786 4800)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#define NOMINMAX
#define NOGDI
#define WIN32_LEAN_AND_MEAN

#include "AvstpFinder.h"

#include <cassert>
#include <cstring>
#include <cwchar>

#if defined (_MSC_VER) && (_MSC_VER >= 1300)    // for VC 7.0
	// from ATL 7.0 sources
	#ifndef _delayimp_h
		extern "C" IMAGE_DOS_HEADER __ImageBase;
	#endif
#endif



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



void	AvstpFinder::publish_lib (::HMODULE hinst)
{
	assert (hinst != 0);

	// Mapped file and mutex names
	wchar_t        mf_name_0 [MAX_PATH];
	wchar_t        mu_name_0 [MAX_PATH];
	compose_mapped_filename (mf_name_0, mu_name_0);

	// Mutex
	::HANDLE       mutex_hnd = ::CreateMutexW (0, TRUE, mu_name_0);
	::DWORD        mutex_err = ::GetLastError ();
	if (mutex_hnd != 0)
	{
		if (mutex_err == ERROR_ALREADY_EXISTS)
		{
			::CloseHandle (mutex_hnd);
		}

		else
		{
			// DLL path
			wchar_t        dll_path_0 [BUFFER_LEN] = { L'\0' };
			::GetModuleFileNameW (hinst, dll_path_0, BUFFER_LEN);	// Ignores result

			// Stores the path into the file
			bool           done_flag = false;
			const int      filesize = sizeof (dll_path_0);
			::HANDLE       file_hnd = 0;
			if (mutex_hnd != 0)
			{
				file_hnd = ::CreateFileMappingW (
					INVALID_HANDLE_VALUE,
					0,
					PAGE_READWRITE,
					0, filesize,
					mf_name_0
				);
				const ::DWORD  last_err = ::GetLastError ();

				if (file_hnd != 0 && last_err == ERROR_ALREADY_EXISTS)
				{
					::CloseHandle (file_hnd);
					file_hnd = 0;
				}
			}

			if (file_hnd != 0)
			{
				void *         buf_ptr = ::MapViewOfFile (
					file_hnd,
					FILE_MAP_ALL_ACCESS,
					0, 0,
					filesize
				);

				if (buf_ptr != 0)
				{
					::CopyMemory (buf_ptr, dll_path_0, filesize);
					done_flag = true;
					::UnmapViewOfFile (buf_ptr);
				}
			}

			if (! done_flag && file_hnd != 0)
			{
				::CloseHandle (file_hnd);
				file_hnd = 0;
			}

			// We don't destroy the mutex, we only release it
			::ReleaseMutex (mutex_hnd);
		}
	}
}



// Returns 0 if not found or on error.
::HMODULE	AvstpFinder::find_lib ()
{
	::HMODULE      dll_hnd = 0;

	// We look first for the published path, if it exists.
	wchar_t        mf_name_0 [MAX_PATH];
	wchar_t        mu_name_0 [MAX_PATH];
	compose_mapped_filename (mf_name_0, mu_name_0);

	// Mutex
	::HANDLE       mutex_hnd = ::OpenMutexW (SYNCHRONIZE, TRUE, mu_name_0);
	if (mutex_hnd != 0)
	{
		::DWORD        wait_ret = ::WaitForSingleObject (mutex_hnd, INFINITE);
		if (wait_ret != WAIT_FAILED)
		{
			// Mapped file
			::HANDLE       file_hnd = ::OpenFileMappingW (
				FILE_MAP_READ,
				0,
				mf_name_0
			);
			if (file_hnd != 0)
			{
				wchar_t        dll_path_0 [BUFFER_LEN] = { L'\0' };
				const int      filesize = sizeof (dll_path_0);
				void *         buf_ptr = ::MapViewOfFile (
					file_hnd,
					FILE_MAP_READ,
					0, 0,
					filesize
				);
				if (buf_ptr != 0)
				{
					::CopyMemory (dll_path_0, buf_ptr, filesize);
					::UnmapViewOfFile (buf_ptr);

					dll_path_0 [BUFFER_LEN - 1] = L'\0';
					dll_hnd = ::LoadLibraryW (dll_path_0);
				}

				::CloseHandle (file_hnd);
				file_hnd = 0;
			}

			::ReleaseMutex (mutex_hnd);
		}

		::CloseHandle (mutex_hnd);
	}

	// Then we try to load the library with the standard search strategy
	if (dll_hnd == 0)
	{
		dll_hnd = ::LoadLibraryW (_lib_name_0);
	}

	// Finally, try with the same path as the calling module
	if (dll_hnd == 0)
	{
		const ::HMODULE   this_hnd = get_code_module ();

		if (this_hnd != 0)
		{
			const int      buf_len = 32767+1;
			wchar_t        dll_path_0 [buf_len] = { L'\0' };
			const ::DWORD  res_gmfnw =
				::GetModuleFileNameW (this_hnd, dll_path_0, buf_len);
			if (int (res_gmfnw) < buf_len)
			{
				wchar_t *      backslash_0 = ::wcsrchr (dll_path_0, L'\\');
				if (backslash_0 != 0)
				{
					wchar_t *      name_0 = backslash_0 + 1;
					const size_t   count_max = dll_path_0 + buf_len - name_0;
#if defined (_MSC_VER)
					::wcsncpy_s (name_0, count_max, _lib_name_0, count_max);
#else
					std::wcsncpy (name_0, _lib_name_0, count_max);
#endif
					dll_path_0 [buf_len - 1] = L'\0';
					dll_hnd = ::LoadLibraryW (dll_path_0);
				}
			}
		}
	}

	return (dll_hnd);
}



const wchar_t	AvstpFinder::_lib_name_0 [] = L"avstp.dll";



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



// mf_name_0 and mu_name_0 must have at least MAX_PATH characters allocated.
void	AvstpFinder::compose_mapped_filename (wchar_t mf_name_0 [], wchar_t mu_name_0 [])
{
	static const wchar_t	base_0 [] = L"Local\\avstp_dll_info_";
	int            pos = 0;
	while (base_0 [pos] != L'\0')
	{
		mf_name_0 [pos] = base_0 [pos];
		mu_name_0 [pos] = base_0 [pos];
		++ pos;
	}

	const ::DWORD  proc_id = ::GetCurrentProcessId ();
	for (int i = sizeof (proc_id) * 2 - 1; i >= 0; --i)
	{
		const int		h = (proc_id >> (i * 4)) & 15;
		const wchar_t	c = wchar_t ((h < 10) ? h + L'0' : h - 10 + L'A');
		mf_name_0 [pos] = c;
		mu_name_0 [pos] = c;
		++ pos;
	}

	mf_name_0 [pos] = L'_';  mu_name_0 [pos] = L'_';  ++ pos;
	mf_name_0 [pos] = L'f';  mu_name_0 [pos] = L'm';  ++ pos;
	mf_name_0 [pos] = L'\0'; mu_name_0 [pos] = L'\0'; ++ pos;
	assert (pos <= MAX_PATH);
}



// Can return 0 if not available.
::HMODULE	AvstpFinder::get_code_module ()
{
	::HMODULE      this_hnd = 0;
	static int     dummy;

	::HMODULE      kernel32_hnd = LoadLibraryW (L"Kernel32.dll");
	if (kernel32_hnd != 0)
	{
		typedef ::BOOL (WINAPI *GmhewPtr) (
			::DWORD dwFlags,
			::LPCWSTR lpModuleName,
			::HMODULE* phModule
		);
		GmhewPtr       gmhew_ptr = reinterpret_cast <GmhewPtr> (::GetProcAddress (
			reinterpret_cast < ::HMODULE> (kernel32_hnd),
			"GetModuleHandleExW"
		));
		if (gmhew_ptr != 0)
		{
			const ::BOOL   res_gmhew = gmhew_ptr (
				  0x00000004	//   GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
				| 0x00000002,	// | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
				(LPCWSTR) (&dummy),	// Acutally any object from this module
				&this_hnd
			);
			if (res_gmhew == 0)
			{
				this_hnd = 0;
			}
		}
	}

	if (this_hnd == 0)
	{
#if ! defined (_MSC_VER) || (_MSC_VER < 1300)	// earlier than .NET compiler (VC 6.0)
		// Here's a trick that will get you the handle of the module
		// you're running in without any a-priori knowledge:
		// http://www.dotnet247.com/247reference/msgs/13/65259.aspx
		::MEMORY_BASIC_INFORMATION	mbi;
		::VirtualQuery (&dummy, &mbi, sizeof (mbi));
		this_hnd = reinterpret_cast < ::HMODULE> (mbi.AllocationBase);
#else    // VC 7.0
		this_hnd = reinterpret_cast < ::HMODULE> (&__ImageBase);
#endif
	}

	return (this_hnd);
}



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
