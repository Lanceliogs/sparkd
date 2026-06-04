/*
 * spark_launcher.c — GUI-subsystem launcher for sparkd (Windows)
 *
 * Compiled with -mwindows so no console window appears.
 * Starts sparkd daemon, spark-ui, then opens the browser.
 *
 * Compile (MinGW):
 *   windres spark_launcher.rc -O coff -o spark_launcher.res
 *   gcc -O2 -mwindows -static -o spark-launcher.exe spark_launcher.c spark_launcher.res
 */

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "consts.h"

static int s_run(const char *exe_dir, const char *args, int wait)
{
    char sparkctl[MAX_PATH];
    snprintf(sparkctl, sizeof(sparkctl), "%s\\sparkctl.exe", exe_dir);

    char cmdline[1024];
    snprintf(cmdline, sizeof(cmdline), "\"%s\" %s", sparkctl, args);

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, exe_dir, &si, &pi))
        return -1;

    if (wait)
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code = 1;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (int)exit_code;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    char exe_dir[MAX_PATH];
    GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
    char *last_sep = strrchr(exe_dir, '\\');
    if (last_sep) *last_sep = '\0';

    if (s_run(exe_dir, "daemon up", 1) != 0)
    {
        MessageBoxA(NULL, "Failed to start sparkd daemon.",
                    "Sparkd", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (s_run(exe_dir, "ui up", 1) != 0)
    {
        MessageBoxA(NULL, "Failed to start spark-ui.",
                    "Sparkd", MB_OK | MB_ICONERROR);
        return 1;
    }

    s_run(exe_dir, "ui open", 0);

    return 0;
}

#else
#include <stdio.h>
int main(void)
{
    fprintf(stderr, "spark-launcher is Windows-only.\n");
    return 1;
}
#endif
