#ifndef DLL_HELPER_H
#define DLL_HELPER_H

#include "core/OriResult.h"

#include <windows.h>

using SdkResult = Ori::Result<HMODULE>;
SdkResult loadSdk(const char* logId, const QString& sdkDir, const QString& mainDll);

using ProcResult = Ori::Result<FARPROC>;
ProcResult resolveProc(const char* logId, HMODULE hLib, const char* name);

#endif // DLL_HELPER_H
