#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "timeout.h"

// ================== CONFIGURATION ==================
#define UPDATE_CHECK_INTERVAL  1000 // 5 seconds
#define NEW_UPDATE_FILE        "demo_update.exe"   // Name of the new exe in same folder

// Global flag to prevent multiple update attempts
static volatile BOOL g_updateInProgress = FALSE;

// ================== SELF UPDATE FUNCTION ==================
BOOL PerformSelfUpdate(const char* newExePath)
{
    char currentExe[MAX_PATH];
    char batchPath[MAX_PATH];
    char tempDir[MAX_PATH];
    char cmdLine[1024];
    GetModuleFileNameA(NULL, currentExe, MAX_PATH);
    GetTempPathA(MAX_PATH, tempDir);
    sprintf_s(batchPath, MAX_PATH, "%supdate_%u.bat", tempDir, GetCurrentProcessId());
    FILE* f = fopen(batchPath, "w");
    if (!f) return FALSE;
    printf("%s -> %s", newExePath, currentExe);
    timeout(10);

    //    "timeout /t 2 >nul\r\n"
    
    //    "tasklist /fi \"PID eq %u\" 2>nul | find \"%u\" >nul\r\n"
    //    "if %ERRORLEVEL%==0 (\r\n"
    //    "    timeout /t 1 >nul\r\n"
    //    "    goto wait\r\n"
    //    ")\r\n"

    //    "del \"%%~f0\" >nul\r\n",
    fprintf(f,
        "@echo off\r\n"
        "timeout /t 2 >nul\r\n"
        "del \"%s\" >nul 2>&1\r\n"
        "copy /Y \"%s\" \"%s\" >nul\r\n"
        "start \"\" \"%s\"\r\n"
        "del \"%s\" >nul\r\n",
        currentExe,
        newExePath,
        currentExe,
        currentExe,
        newExePath);
    fclose(f);
    // Launch hidden
    sprintf_s(cmdLine, sizeof(cmdLine), "start /min cmd.exe /c \"%s\"", batchPath);
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL success = CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
                                 CREATE_NO_WINDOW | DETACHED_PROCESS,
                                 NULL, NULL, &si, &pi);
    if (success) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return success;
}

// ================== BACKGROUND UPDATE THREAD ==================
DWORD WINAPI UpdateCheckerThread(LPVOID lpParam)
{
    char newFilePath[MAX_PATH];
    GetModuleFileNameA(NULL, newFilePath, MAX_PATH);
    // Replace filename with update filename
    char* lastSlash = strrchr(newFilePath, '\\');
    if (lastSlash) {
        sprintf_s(lastSlash + 1, MAX_PATH - (lastSlash - newFilePath + 1), NEW_UPDATE_FILE);
    } else {
        strcpy_s(newFilePath, MAX_PATH, NEW_UPDATE_FILE);
    }
    printf("Update checker thread started. Watching for: %s\n", newFilePath);
    while (TRUE)
    {
        if (!g_updateInProgress)
        {
            // Check if update file exists
            if (GetFileAttributesA(newFilePath) != INVALID_FILE_ATTRIBUTES)
            {
                printf("New update detected! Starting self-update...\n");
                g_updateInProgress = TRUE;
                if (PerformSelfUpdate(newFilePath))
                {
                    // Give a small delay before exiting so the batch can take over
                    Sleep(800);
                    ExitProcess(0);
                }
                else
                {
                    printf("Failed to start update process.\n");
                    g_updateInProgress = FALSE;
                }
            }
        }
        Sleep(UPDATE_CHECK_INTERVAL);
    }
    return 0;
}

void Updater() {
  printf("Self-update thread enabled.\n");
  // Create background update checker thread
  HANDLE hThread = CreateThread(NULL, 0, UpdateCheckerThread, NULL, 0, NULL);
  if (hThread) {
    CloseHandle(hThread);  // We don't need to wait for it
  } else {
    printf("Failed to create update thread.\n");
  }
}

// ================== MAIN ==================
// int main(void)
// {
//     printf("Program started. Self-update thread enabled.\n");
//     // Create background update checker thread
//     HANDLE hThread = CreateThread(NULL, 0, UpdateCheckerThread, NULL, 0, NULL);
//     if (hThread) {
//         CloseHandle(hThread);  // We don't need to wait for it
//     } else {
//         printf("Failed to create update thread.\n");
//     }
//     // =====================
//     // Your normal program code goes here
//     // =====================
//     while (TRUE)   // Example main loop
//     {
//         // Your application logic...
//         Sleep(1000);
//     }
//     return 0;
// }

// #include <windows.h>
// #include <stdio.h>
// #include <stdlib.h>
// 
// BOOL PerformSelfUpdate(const char* newExePath)
// {
//     char currentExe[MAX_PATH];
//     char batchPath[MAX_PATH];
//     char tempDir[MAX_PATH];
//     char cmdLine[1024];
//     // Get current executable path
//     GetModuleFileNameA(NULL, currentExe, MAX_PATH);
//     // Get temp directory
//     GetTempPathA(MAX_PATH, tempDir);
//     // Create paths
//     // sprintf_s(batchPath, MAX_PATH, "%supdate_%u.bat", tempDir, GetCurrentProcessId());
//     const char* newName = "demo_new.exe";  // or extract from newExePath
//     // Create the helper batch file
//     FILE* f = fopen(batchPath, "w");
//     if (!f) return FALSE;
//     fprintf(f,
//         "@echo off\r\n"
//         "timeout /t 2 /nobreak >nul\r\n"                    // Small delay
//         ":wait\r\n"
//         "tasklist /fi \"PID eq %u\" | find \":\" >nul\r\n"  // Wait for this process to die
//         "if %%ERRORLEVEL%%==0 (\r\n"
//         "    timeout /t 1 /nobreak >nul\r\n"
//         "    goto wait\r\n"
//         ")\r\n"
//         "copy /Y \"%s\" \"%s\" >nul\r\n"                    // Replace old with new
//         "del \"%s\" >nul\r\n"                               // Delete temporary new file
//         "start \"\" \"%s\"\r\n"                             // Start new version
//         "del \"%%~f0\" >nul\r\n",                           // Self-delete this batch
//         GetCurrentProcessId(),
//         newExePath,
//         currentExe,
//         newExePath,
//         currentExe);
//     fclose(f);
//     // Launch the batch file detached
//     sprintf_s(cmdLine, sizeof(cmdLine), "cmd.exe /c \"%s\"", batchPath);
//     STARTUPINFOA si = { sizeof(si) };
//     PROCESS_INFORMATION pi;
//     if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
//                        CREATE_NO_WINDOW | DETACHED_PROCESS,
//                        NULL, NULL, &si, &pi))
//     {
//         CloseHandle(pi.hProcess);
//         CloseHandle(pi.hThread);
//         return TRUE;
//     }
//     return FALSE;
// }
// 
// static int main(void)
// {
//     // ... your normal program code ...
//     // When update is ready:
//     const char* downloadedNewExe = "demo_new.exe";
//     if (PerformSelfUpdate(downloadedNewExe))
//     {
//         // Exit cleanly so the updater can replace us
//         ExitProcess(0);
//     }
//     else
//     {
//         printf("Update failed!\n");
//     }
//     return 0;
// }
