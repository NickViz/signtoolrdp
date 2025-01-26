// Copyright (C) 2025, VorontSoft
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include "windows.h"
#include "detours.h"


//https://stackoverflow.com/questions/15906740/how-to-use-an-ev-code-signing-certificate-on-a-virtual-machine-to-sign-an-msi
//Each time you connect via RDP to this machine the dongle is no longer 
//accessible, because the SmartCardAPI will redirect the access.
//
//There is a solution to this problem however: The SmartCard stack uses the API-Calls
//ProcessIdToSessionId
//WinStationGetCurrentSessionCapabilities
//
//to determine if the current process is running in a RDP session.
//By injecting a DLL into the signtool and using the Detours framework 
//you can hook these API calls and report a local session, so the SmardCardAPI
//will access the dongle connected to the remote virtual machine.
// 
// PE editor: https://github.com/hasherezade/pe-bear/releases/

#define DETOUR_SESSION_CAPABILITIES
//#define DETOUR_SESSION_ID

#if !defined(DETOUR_SESSION_CAPABILITIES) && !defined(DETOUR_SESSION_ID)
#error At least one option must be selected!
#endif

#define FUNC_BOOL  extern "C" __declspec(dllexport) BOOL  APIENTRY
#define FUNC_DWORD extern "C" __declspec(dllexport) DWORD APIENTRY

FUNC_DWORD A_sample() // Export #0 ordinal
{
	return 0;
}

#ifdef DETOUR_SESSION_CAPABILITIES
static HMODULE hStaDLL = NULL;
static BOOL (WINAPI *TrueGetCurStationCap)(DWORD, DWORD*) = NULL;

FUNC_BOOL WinStationGetCurrentSessionCapabilitiesLocal(DWORD flags, DWORD* pOutBuffer)
{
	OutputDebugString(L"Detoured WinStationGetCurrentSessionCapabilities\r\n");
	BOOL bResult = TrueGetCurStationCap(flags, pOutBuffer);
	if (bResult)
		*pOutBuffer = 0;
	return bResult;
}
#endif

#ifdef DETOUR_SESSION_ID
static HMODULE hKernelDLL = NULL;
static DWORD(WINAPI* TrueProcessIdToSessionId)() = NULL;

FUNC_DWORD ProcessIdToSessionIdLocal(DWORD dwProcessId, DWORD* pSessionId)
{
	OutputDebugString(L"Detoured ProcessIdToSessionId\r\n");
	if (pSessionId)
		*pSessionId = 0;
	return TRUE;
}
#endif

FUNC_BOOL DllMain(HMODULE haDLL, DWORD dwReason, LPVOID)
{
	wchar_t cBuffer[4096];
	wsprintf(cBuffer, L"DllMain Entry %08x\r\n", dwReason);
	OutputDebugString(cBuffer);
	if (DetourIsHelperProcess())
		return TRUE;

	if (dwReason == DLL_PROCESS_ATTACH) 
	{
		OutputDebugString(L"Starting Detour API Calls\r\n");
#ifdef DETOUR_SESSION_ID
		hKernelDLL = LoadLibrary(L"KERNEL32.DLL");
		if (hKernelDLL)
			TrueProcessIdToSessionId = (DWORD(WINAPI *)())
				GetProcAddress(hKernelDLL, "ProcessIdToSessionId");
		if (TrueProcessIdToSessionId == NULL)
		{
			OutputDebugString(L"Unable to get ProcessIdToSessionId address\r\n");
			return FALSE;
		}
#endif

#ifdef DETOUR_SESSION_CAPABILITIES
		hStaDLL = LoadLibrary(L"WINSTA.DLL");
		if (hStaDLL)
			TrueGetCurStationCap = (BOOL(WINAPI *)(DWORD, DWORD*))
				GetProcAddress(hStaDLL, "WinStationGetCurrentSessionCapabilities");
		if (TrueGetCurStationCap == NULL)
		{
			OutputDebugString(L"Unable to get WinStationGetCurrentSessionCapabilities address\r\n");
			return FALSE;
		}
#endif
		DetourRestoreAfterWith();
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
#ifdef DETOUR_SESSION_ID
		DetourAttach((PVOID*)&TrueProcessIdToSessionId, ProcessIdToSessionIdLocal);
#endif
#ifdef DETOUR_SESSION_CAPABILITIES
		DetourAttach((PVOID*)&TrueGetCurStationCap, WinStationGetCurrentSessionCapabilitiesLocal);
#endif
		if (DetourTransactionCommit() == NO_ERROR)
			OutputDebugString(L"Successfully Detoured API Calls\r\n");
		else
			return FALSE;
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
#ifdef DETOUR_SESSION_CAPABILITIES
		DetourDetach((PVOID*)&TrueGetCurStationCap, WinStationGetCurrentSessionCapabilitiesLocal);
#endif
#ifdef DETOUR_SESSION_ID
		DetourDetach((PVOID*)&TrueProcessIdToSessionId, ProcessIdToSessionIdLocal);
#endif
		if (DetourTransactionCommit() == NO_ERROR)
			OutputDebugString(L"Successfully reverted API Calls\r\n");

#ifdef DETOUR_SESSION_CAPABILITIES
		FreeLibrary(hStaDLL);
		hStaDLL = NULL;
#endif
#ifdef DETOUR_SESSION_ID
		FreeLibrary(hKernelDLL);
		hKernelDLL = NULL;
#endif
	}
	return TRUE;
} // FUNC_BOOL DllMain(HMODULE haDLL, DWORD dwReason, LPVOID)
