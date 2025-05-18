#include <windows.h>
#include <stdio.h>
#include <string>
#include <iostream>

// Function to print usage information
void PrintUsage(const char* programName) {
    printf("Usage: %s <path_to_file>\n", programName);
    printf("  <path_to_file> - Path to the EXE or DLL file to extract version information from\n");
}

// Function to get version information from a PE file
bool GetVersionInfo(const char* filePath) {
    DWORD dwHandle;
    DWORD dwSize = GetFileVersionInfoSizeA(filePath, &dwHandle);
    
    if (dwSize == 0) {
        DWORD error = GetLastError();
        printf("Error: Could not get version info size. Error code: %lu\n", error);
        return false;
    }
    
    // Allocate memory for the version info
    LPVOID lpData = malloc(dwSize);
    if (!lpData) {
        printf("Error: Memory allocation failed\n");
        return false;
    }
    
    // Get the version info
    if (!GetFileVersionInfoA(filePath, 0, dwSize, lpData)) {
        DWORD error = GetLastError();
        printf("Error: Could not get version info. Error code: %lu\n", error);
        free(lpData);
        return false;
    }
    
    // Get the fixed file info structure
    VS_FIXEDFILEINFO* pFileInfo;
    UINT uLen;
    if (!VerQueryValueA(lpData, "\\", (LPVOID*)&pFileInfo, &uLen)) {
        printf("Error: Could not query version info value\n");
        free(lpData);
        return false;
    }
    
    // Check if we got the VS_FIXEDFILEINFO structure
    if (uLen == 0) {
        printf("Error: No fixed file info found\n");
        free(lpData);
        return false;
    }
    
    // Extract and print the version numbers
    DWORD dwFileVersionMS = pFileInfo->dwFileVersionMS;
    DWORD dwFileVersionLS = pFileInfo->dwFileVersionLS;
    
    WORD wMajor = HIWORD(dwFileVersionMS);
    WORD wMinor = LOWORD(dwFileVersionMS);
    WORD wBuild = HIWORD(dwFileVersionLS);
    WORD wRevision = LOWORD(dwFileVersionLS);
    
    printf("%u.%u.%u.%u\n", wMajor, wMinor, wBuild, wRevision);
    
    // Free the allocated memory
    free(lpData);
    return true;
}

int main(int argc, char* argv[]) {
    // Check if a file path was provided
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    const char* filePath = argv[1];
    
    // Get the version info
    if (!GetVersionInfo(filePath)) {
        return 1;
    }
    
    return 0;
}
