// API shims for Windows XP compatibility
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <stdio.h>
#include <string.h>

extern "C" {

// Creates a fake import (basically a function pointer) for the function to override the DLL import with our own implementation
#define DLL_OVERRIDE(name, argbytes) typeof(name) *name##_ptr asm("__imp__"#name"@"#argbytes) = name;

static LCID locale_name_to_id(LPCWSTR lpLocaleName)
{
    if (lpLocaleName == LOCALE_NAME_USER_DEFAULT)  // LOCALE_NAME_USER_DEFAULT is equal to NULL
        return LOCALE_USER_DEFAULT;
    else if (wcscmp(lpLocaleName, LOCALE_NAME_INVARIANT) == 0)
        return LOCALE_INVARIANT;
    else if (wcscmp(lpLocaleName, LOCALE_NAME_SYSTEM_DEFAULT) == 0)
        return LOCALE_SYSTEM_DEFAULT;
    return 0xFFFFFFFF;  // something invalid
}

int WINAPI GetDateFormatEx(
  LPCWSTR          lpLocaleName,
  DWORD            dwFlags,
  const SYSTEMTIME *lpDate,
  LPCWSTR          lpFormat,
  LPWSTR           lpDateStr,
  int              cchDate,
  LPCWSTR          lpCalendar)
{
    wprintf(L"shim: GetDateFormatEx(\"%s\", %u, %p, \"%s\", %p, %i, \"%s\")\n", lpLocaleName, dwFlags, lpDate, lpFormat, lpDateStr, cchDate, lpCalendar);
    return GetDateFormatW(locale_name_to_id(lpLocaleName), dwFlags, lpDate, lpFormat, lpDateStr, cchDate);
}

DLL_OVERRIDE(GetDateFormatEx,28)

int WINAPI GetTimeFormatEx(
  LPCWSTR          lpLocaleName,
  DWORD            dwFlags,
  const SYSTEMTIME *lpTime,
  LPCWSTR          lpFormat,
  LPWSTR           lpTimeStr,
  int              cchTime)
{
    wprintf(L"shim: GetTimeFormatEx(\"%s\", %u, %p, \"%s\", %p, %i)\n", lpLocaleName, dwFlags, lpTime, lpFormat, lpTimeStr, cchTime);
    return GetTimeFormatW(locale_name_to_id(lpLocaleName), dwFlags, lpTime, lpFormat, lpTimeStr, cchTime);
}

DLL_OVERRIDE(GetTimeFormatEx,24)

int WINAPI GetLocaleInfoEx(
  LPCWSTR lpLocaleName,
  LCTYPE  LCType,
  LPWSTR  lpLCData,
  int     cchData)
{
    wprintf(L"shim: GetLocaleInfoEx(\"%s\", %u, %p, %i)\n", lpLocaleName, LCType, lpLCData, cchData);
    return GetLocaleInfoW(locale_name_to_id(lpLocaleName), LCType, lpLCData, cchData);
}

DLL_OVERRIDE(GetLocaleInfoEx,16)

int WINAPI LCMapStringEx(
  LPCWSTR          lpLocaleName,
  DWORD            dwMapFlags,
  LPCWSTR          lpSrcStr,
  int              cchSrc,
  LPWSTR           lpDestStr,
  int              cchDest,
  LPNLSVERSIONINFO lpVersionInformation,
  LPVOID           lpReserved,
  LPARAM           sortHandle)
{
    wprintf(L"shim: LCMapStringEx(\"%s\", %u, \"%s\", %i, %p, %i)\n", lpLocaleName, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest);
    return LCMapStringW(locale_name_to_id(lpLocaleName), dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest);
}

DLL_OVERRIDE(LCMapStringEx,36)

LSTATUS WINAPI RegDeleteTreeW(
  HKEY    hKey,
  LPCWSTR lpSubKey)
{
    wprintf(L"shim: RegDeleteTreeW(0x%X, \"%s\")\n", hKey, lpSubKey);
    // Note: SHDeleteKey will also remove the key itself, while RegDeleteTree just removes its contents.
    return SHDeleteKeyW(hKey, lpSubKey);
}

DLL_OVERRIDE(RegDeleteTreeW,8)

LSTATUS WINAPI RegSetKeyValueW(
  HKEY    hKey,
  LPCWSTR lpSubKey,
  LPCWSTR lpValueName,
  DWORD   dwType,
  LPCVOID lpData,
  DWORD   cbData
)
{
	wprintf(L"shim (unimplemented): RegSetKeyValueW(0x%x, \"%s\", \"%s\", %u, %p, %u)\n", hKey, lpSubKey, lpValueName, dwType, lpData, cbData);
	// TODO: implement
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DLL_OVERRIDE(RegSetKeyValueW,24)

HRESULT WINAPI SHGetKnownFolderPath(
  REFKNOWNFOLDERID rfid,
  DWORD            dwFlags,
  HANDLE           hToken,
  PWSTR            *ppszPath)
{
    wprintf(L"shim: SHGetKnownFolderPath(%p, %u, %p, %p)\n", rfid, dwFlags, hToken, ppszPath);
    int csidl = -1;
    if (rfid == FOLDERID_Documents)         csidl = CSIDL_MYDOCUMENTS;
    else if (rfid == FOLDERID_Fonts)        csidl = CSIDL_FONTS;
    else if (rfid == FOLDERID_LocalAppData) csidl = CSIDL_LOCAL_APPDATA;
    else if (rfid == FOLDERID_Profile)      csidl = CSIDL_PROFILE;
    *ppszPath = (PWSTR)CoTaskMemAlloc(MAX_PATH * sizeof(WCHAR));
    return SHGetFolderPathW(NULL, csidl, hToken, SHGFP_TYPE_CURRENT, *ppszPath);
}

DLL_OVERRIDE(SHGetKnownFolderPath,16)

BOOL WINAPI CancelIoEx(
  HANDLE       hFile,
  LPOVERLAPPED lpOverlapped)
{
    printf("shim: CancelIoEx(%p, %p)\n", hFile, lpOverlapped);
    // TODO: CancelIo does not work on handles created on a different thread!
    // Not sure if that functionality is needed.
    return CancelIo(hFile);
}

DLL_OVERRIDE(CancelIoEx,8)

}
