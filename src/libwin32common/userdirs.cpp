/***************************************************************************
 * ROM Properties Page shell extension. (libwin32common)                   *
 * userdirs.cpp: Find user directories.                                    *
 *                                                                         *
 * Copyright (c) 2016-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

// NOTE: All functions return 8-bit strings.
// This is usually encoded as UTF-8.
#include "userdirs.hpp"

// C includes. (C++ namespace)
#include <cassert>
#include <cstring>

// C++ includes.
#include <string>
using std::string;

// Windows SDK.
#include "RpWin32_sdk.h"
#include <shlobj.h>

namespace LibWin32Common {

/**
 * Internal W2U8() function.
 * @param wcs TCHAR string.
 * @return UTF-8 C++ string.
 */
static inline string W2U8(const WCHAR *wcs)
{
	string s_ret;

	// NOTE: cbMbs includes the NULL terminator.
	int cbMbs = WideCharToMultiByte(CP_UTF8, 0, wcs, -1, nullptr, 0, nullptr, nullptr);
	if (cbMbs <= 1) {
		return s_ret;
	}
	cbMbs--;
 
	char *mbs = static_cast<char*>(malloc(cbMbs));
	WideCharToMultiByte(CP_UTF8, 0, wcs, -1, mbs, cbMbs, nullptr, nullptr);
	s_ret.assign(mbs, cbMbs);
	free(mbs);
	return s_ret;
}
#ifdef UNICODE
# define T2U8(wcs) W2U8(wcs)
#else /* !UNICODE */
// TODO: Convert ANSI to UTF-8?
# define T2U8(mbs) (mbs)
#endif /* UNICODE */

/**
 * Get the user's home directory.
 *
 * NOTE: This function does NOT cache the directory name.
 * Callers should cache it locally.
 *
 * @return User's home directory (without trailing slash), or empty string on error.
 */
string getHomeDirectory(void)
{
	string home_dir;
	TCHAR path[MAX_PATH];
	HRESULT hr;

	// Windows: Get CSIDL_PROFILE.
	// - Windows XP: C:\Documents and Settings\username
	// - Windows Vista: C:\Users\username
	hr = SHGetFolderPath(nullptr, CSIDL_PROFILE,
		nullptr, SHGFP_TYPE_CURRENT, path);
	if (hr == S_OK) {
		home_dir = T2U8(path);
		if (!home_dir.empty()) {
			// Add a trailing backslash if necessary.
			if (home_dir.at(home_dir.size()-1) != '\\') {
				home_dir += '\\';
			}
		}
	}

	return home_dir;
}

/**
 * Get the user's cache directory.
 *
 * NOTE: This function does NOT cache the directory name.
 * Callers should cache it locally.
 *
 * @return User's cache directory (without trailing slash), or empty string on error.
 */
string getCacheDirectory(void)
{
	string cache_dir;
	TCHAR szPath[MAX_PATH];

	// shell32.dll might be delay-loaded to avoid a gdi32.dll penalty.
	// Call SHGetFolderPath() with invalid parameters to load it into
	// memory before using GetModuleHandle().
	SHGetFolderPath(nullptr, 0, nullptr, 0, szPath);

	// Windows: Get FOLDERID_LocalAppDataLow. (WinXP and earlier: CSIDL_LOCAL_APPDATA)
	// LocalLow is preferred because it allows rp-download to run as a
	// low-integrity process on Windows Vista and later.
	// - Windows XP: C:\Documents and Settings\username\Local Settings\Application Data
	// - Windows Vista: C:\Users\username\AppData\LocalLow
	HMODULE hShell32_dll = GetModuleHandle(_T("shell32.dll"));
	assert(hShell32_dll != nullptr);
	if (!hShell32_dll) {
		// Not possible. Both SHGetFolderPath() and
		// SHGetKnownFolderPath() are in shell32.dll.
		return cache_dir;
	}

	// Check for SHGetKnownFolderPath. (Windows Vista and later)
	typedef HRESULT (WINAPI *PFNSHGETKNOWNFOLDERPATH)(
		_In_ REFKNOWNFOLDERID rfid,
		_In_ DWORD /* KNOWN_FOLDER_FLAG */ dwFlags,
		_In_opt_ HANDLE hToken,
		_Outptr_ PWSTR *ppszPath);
	PFNSHGETKNOWNFOLDERPATH pfnSHGetKnownFolderPath =
		(PFNSHGETKNOWNFOLDERPATH)GetProcAddress(hShell32_dll, "SHGetKnownFolderPath");
	if (pfnSHGetKnownFolderPath) {
		// We have SHGetKnownFolderPath. (NOTE: Unicode only!)
		// TODO: Get LocalLow. For now, we'll get Local.
		PWSTR pszPath = nullptr;	// free with CoTaskMemFree()
		HRESULT hr = pfnSHGetKnownFolderPath(FOLDERID_LocalAppDataLow,
			SHGFP_TYPE_CURRENT, nullptr, &pszPath);
		if (SUCCEEDED(hr) && pszPath != nullptr) {
			// Path obtained.
			cache_dir = W2U8(pszPath);
		}
		if (pszPath) {
			CoTaskMemFree(pszPath);
			pszPath = nullptr;
		}

		if (cache_dir.empty()) {
			// SHGetKnownFolderPath(FOLDERID_LocalAppDataLow) failed.
			// Try again with FOLDERID_LocalAppData.
			// NOTE: This might cause problems if rp-download runs
			// with a low integrity level.
			hr = pfnSHGetKnownFolderPath(FOLDERID_LocalAppData,
				SHGFP_TYPE_CURRENT, nullptr, &pszPath);
			if (SUCCEEDED(hr) && pszPath != nullptr) {
				// Path obtained.
				cache_dir = W2U8(pszPath);
			}
			if (pszPath) {
				CoTaskMemFree(pszPath);
				pszPath = nullptr;
			}
		}
	}

	if (cache_dir.empty()) {
		// SHGetKnownFolderPath() either isn't available
		// or failed. Fall back to SHGetFolderPath().
		szPath[0] = _T('\0');
		HRESULT hr = SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA,
			nullptr, SHGFP_TYPE_CURRENT, szPath);
		if (SUCCEEDED(hr)) {
			cache_dir = T2U8(szPath);
		}
	}

	// We're done trying to obtain the cache directory.
	if (!cache_dir.empty()) {
		// Add a trailing backslash if necessary.
		if (cache_dir.at(cache_dir.size()-1) != '\\') {
			cache_dir += '\\';
		}
	}

	return cache_dir;
}

/**
 * Get the user's configuration directory.
 *
 * NOTE: This function does NOT cache the directory name.
 * Callers should cache it locally.
 *
 * @return User's configuration directory (without trailing slash), or empty string on error.
 */
string getConfigDirectory(void)
{
	string config_dir;
	TCHAR path[MAX_PATH];
	HRESULT hr;

	// Windows: Get CSIDL_APPDATA.
	// - Windows XP: C:\Documents and Settings\username\Application Data
	// - Windows Vista: C:\Users\username\AppData\Roaming
	hr = SHGetFolderPath(nullptr, CSIDL_APPDATA,
		nullptr, SHGFP_TYPE_CURRENT, path);
	if (hr == S_OK) {
		config_dir = T2U8(path);
		if (!config_dir.empty()) {
			// Add a trailing backslash if necessary.
			if (config_dir.at(config_dir.size()-1) != '\\') {
				config_dir += '\\';
			}
		}
	}

	return config_dir;
}

}