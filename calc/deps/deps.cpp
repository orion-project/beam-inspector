#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>

// Function to print usage information
void PrintUsage(const char* programName) {
    printf("Usage: %s <path_to_file>\n", programName);
    printf("  <path_to_file> - Path to the EXE or DLL file to analyze\n");
}

// Function to get the list of DLL dependencies from a PE file
std::vector<std::string> GetDependencies(const char* filePath) {
    std::vector<std::string> dependencies;
    
    // Load the file as a data file, not as an executable
    HMODULE hModule = LoadLibraryExA(filePath, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!hModule) {
        printf("Error: Could not load file. Error code: %lu\n", GetLastError());
        return dependencies;
    }
    
    // Get the DOS header
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    
    // Check if the file is a valid PE file
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        printf("Error: Not a valid PE file\n");
        FreeLibrary(hModule);
        return dependencies;
    }
    
    // Get the NT header
    PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
    
    // Check if the NT header is valid
    if (ntHeader->Signature != IMAGE_NT_SIGNATURE) {
        printf("Error: Not a valid PE file\n");
        FreeLibrary(hModule);
        return dependencies;
    }
    
    // Get the import descriptor
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hModule + 
        ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    
    // Check if there are any imports
    if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size == 0 || 
        ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0) {
        printf("No imports found\n");
        FreeLibrary(hModule);
        return dependencies;
    }
    
    // Iterate through the import descriptors
    while (importDesc->Name != 0) {
        // Get the name of the DLL
        const char* dllName = (const char*)((BYTE*)hModule + importDesc->Name);
        dependencies.push_back(dllName);
        
        // Move to the next import descriptor
        importDesc++;
    }
    
    // Free the loaded module
    FreeLibrary(hModule);
    
    return dependencies;
}

int main(int argc, char* argv[]) {
    // Check if a file path was provided
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    const char* filePath = argv[1];
    
    // Get the dependencies
    std::vector<std::string> dependencies = GetDependencies(filePath);
    
    // Print the dependencies
    if (!dependencies.empty()) {
        printf("Dependencies for %s:\n", filePath);
        for (const auto& dll : dependencies) {
            printf("  %s\n", dll.c_str());
        }
    } else {
        printf("No dependencies found or error occurred.\n");
    }
    
    return 0;
}
