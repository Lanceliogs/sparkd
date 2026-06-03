/*
 * spark_install.c — Self-extracting installer for sparkd (Windows)
 *
 * Embeds all sparkd binaries via cres. On run:
 *   1. Extracts to %LOCALAPPDATA%\sparkd\bin\
 *   2. Adds bin dir to user PATH (HKCU\Environment)
 *   3. Creates Start Menu shortcut
 *   4. Writes uninstall.bat
 */

#ifdef _WIN32

#include "cres.h"
#include "install_resources.h"
#include "../src/consts.h"
#include "../src/fs.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <windows.h>
#include <shlobj.h>

#define INSTALL_DIR_NAME "sparkd"

static char s_install_dir[MAX_PATH];
static char s_bin_dir[MAX_PATH];


static int s_extract_files(void)
{
    int ok = 0, fail = 0;
    for (size_t i = 0; i < cres_table_count; i++)
    {
        CResEntry *e = &cres_table[i];
        if (cres_load(e) != 0)
        {
            fprintf(stderr, "  FAIL: could not decompress %s\n", e->name);
            fail++;
            continue;
        }

        char out_path[MAX_PATH];
        snprintf(out_path, sizeof(out_path), "%s\\%s", s_install_dir, e->name);
        for (char *p = out_path; *p; p++)
            if (*p == '/') *p = '\\';

        /* Ensure parent directory exists */
        char parent[MAX_PATH];
        if (spark_fs_path_parent(parent, sizeof(parent), out_path) == 0)
            spark_fs_mkdir_p(parent);

        FILE *f = fopen(out_path, "wb");
        if (!f)
        {
            fprintf(stderr, "  FAIL: cannot write %s\n", out_path);
            fail++;
            continue;
        }
        fwrite(e->data, 1, e->size, f);
        fclose(f);
        printf("  %s (%lu bytes)\n", e->name, (unsigned long)e->size);
        ok++;
    }

    printf("\n  Extracted %d files", ok);
    if (fail) printf(" (%d failed)", fail);
    printf("\n");
    return fail == 0 ? 0 : -1;
}

static int s_add_to_path(void)
{
    HKEY hkey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0,
                      KEY_READ | KEY_WRITE, &hkey) != ERROR_SUCCESS)
    {
        fprintf(stderr, "  FAIL: cannot open HKCU\\Environment\n");
        return -1;
    }

    char current[8192] = "";
    DWORD size = sizeof(current);
    DWORD type = REG_EXPAND_SZ;
    RegQueryValueExA(hkey, "Path", NULL, &type, (BYTE *)current, &size);

    /* Check if already in PATH */
    if (strstr(current, s_bin_dir))
    {
        printf("  Already in PATH\n");
        RegCloseKey(hkey);
        return 0;
    }

    /* Append */
    char new_path[8192];
    if (current[0])
        snprintf(new_path, sizeof(new_path), "%s;%s", current, s_bin_dir);
    else
        snprintf(new_path, sizeof(new_path), "%s", s_bin_dir);

    if (RegSetValueExA(hkey, "Path", 0, REG_EXPAND_SZ,
                       (BYTE *)new_path, (DWORD)strlen(new_path) + 1) != ERROR_SUCCESS)
    {
        fprintf(stderr, "  FAIL: cannot update PATH\n");
        RegCloseKey(hkey);
        return -1;
    }

    RegCloseKey(hkey);

    /* Notify other programs of the change */
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);

    printf("  Added to user PATH\n");
    return 0;
}

static int s_create_shortcut(void)
{
    char start_menu[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, 0, start_menu) != S_OK)
    {
        fprintf(stderr, "  FAIL: cannot find Start Menu folder\n");
        return -1;
    }

    char shortcut_dir[MAX_PATH];
    snprintf(shortcut_dir, sizeof(shortcut_dir), "%s\\sparkd", start_menu);
    spark_fs_mkdir_p(shortcut_dir);

    char shortcut_path[MAX_PATH];
    snprintf(shortcut_path, sizeof(shortcut_path), "%s\\sparkctl.lnk", shortcut_dir);

    char target[MAX_PATH];
    snprintf(target, sizeof(target), "%s\\sparkctl.exe", s_bin_dir);

    /* Use PowerShell to create the .lnk (avoids COM boilerplate) */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "powershell -NoProfile -Command \""
        "$ws = New-Object -ComObject WScript.Shell; "
        "$sc = $ws.CreateShortcut('%s'); "
        "$sc.TargetPath = '%s'; "
        "$sc.WorkingDirectory = '%s'; "
        "$sc.Description = 'sparkd lighting controller'; "
        "$sc.Save()\"",
        shortcut_path, target, s_bin_dir);

    if (system(cmd) != 0)
    {
        fprintf(stderr, "  FAIL: could not create shortcut\n");
        return -1;
    }

    printf("  Start Menu shortcut created\n");
    return 0;
}

static int s_write_uninstaller(void)
{
    char bat_path[MAX_PATH];
    snprintf(bat_path, sizeof(bat_path), "%s\\uninstall.bat", s_install_dir);

    FILE *f = fopen(bat_path, "w");
    if (!f)
    {
        fprintf(stderr, "  FAIL: cannot write uninstall.bat\n");
        return -1;
    }

    fprintf(f, "@echo off\n");
    fprintf(f, "echo.\n");
    fprintf(f, "echo   sparkd uninstaller\n");
    fprintf(f, "echo   ========================\n");
    fprintf(f, "echo.\n");
    fprintf(f, "echo   Remove: %s\n", s_install_dir);
    fprintf(f, "echo.\n");
    fprintf(f, "pause\n");
    fprintf(f, "echo.\n");

    /* Remove bin and ui folders */
    fprintf(f, "echo [1/3] Removing files...\n");
    fprintf(f, "rmdir /s /q \"%s\\bin\" 2>nul\n", s_install_dir);
    fprintf(f, "rmdir /s /q \"%s\\ui\" 2>nul\n", s_install_dir);

    /* Remove from PATH */
    fprintf(f, "echo [2/3] Removing from PATH...\n");
    fprintf(f, "powershell -NoProfile -Command \""
              "$p = [Environment]::GetEnvironmentVariable('Path','User'); "
              "$p = ($p.Split(';') | Where-Object { $_ -ne '%s' }) -join ';'; "
              "[Environment]::SetEnvironmentVariable('Path',$p,'User')\"\n",
              s_bin_dir);

    /* Remove Start Menu shortcut */
    fprintf(f, "echo [3/3] Removing shortcuts...\n");
    char start_menu[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, 0, start_menu) == S_OK)
    {
        fprintf(f, "rmdir /s /q \"%s\\sparkd\" 2>nul\n", start_menu);
    }

    fprintf(f, "echo.\n");
    fprintf(f, "echo   ========================\n");
    fprintf(f, "echo   sparkd uninstalled.\n");
    fprintf(f, "echo.\n");
    fprintf(f, "pause\n");

    /* Self-delete: copy to temp, run from there to remove install dir */
    fprintf(f, "cd /d \"%%TEMP%%\"\n");
    fprintf(f, "rmdir /s /q \"%s\" 2>nul\n", s_install_dir);
    fprintf(f, "(goto) 2>nul & del \"%%~f0\"\n");

    fclose(f);
    printf("  Uninstaller written: %s\n", bat_path);
    return 0;
}

int main(int argc, char **argv)
{
    int silent = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--version") == 0)
        {
            printf("sparkd-installer, from sparkd v%s\n", SPARKD_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--silent") == 0 || strcmp(argv[i], "-y") == 0)
            silent = 1;
    }

    printf("\n");
    printf("  sparkd installer v%s\n", SPARKD_VERSION);
    printf("  ========================\n\n");

    /* Resolve install path */
    const char *appdata = getenv("LOCALAPPDATA");
    if (!appdata)
    {
        fprintf(stderr, "ERROR: LOCALAPPDATA not set\n");
        return 1;
    }

    snprintf(s_install_dir, sizeof(s_install_dir), "%s\\%s", appdata, INSTALL_DIR_NAME);
    snprintf(s_bin_dir, sizeof(s_bin_dir), "%s\\bin", s_install_dir);

    /* Check for existing installation */
    WIN32_FILE_ATTRIBUTE_DATA attr;
    int upgrading = GetFileAttributesExA(s_bin_dir, GetFileExInfoStandard, &attr);

    printf("  Install to: %s\n", s_install_dir);
    printf("    bin/    executables\n");
    printf("    ui/     web interface\n");
    if (upgrading)
        printf("\n  Existing installation detected — will overwrite.\n");

    if (!silent)
    {
        printf("\n  Press ENTER to install, or Ctrl+C to cancel...");
        fflush(stdout);
        getchar();
    }
    printf("\n");

    /* Extract */
    printf("[1/4] Extracting files...\n");
    if (s_extract_files() != 0)
    {
        fprintf(stderr, "\nInstallation failed.\n");
        return 1;
    }

    /* PATH */
    printf("\n[2/4] Updating PATH...\n");
    s_add_to_path();

    /* Shortcut */
    printf("\n[3/4] Creating Start Menu shortcut...\n");
    s_create_shortcut();

    /* Uninstaller */
    printf("\n[4/4] Writing uninstaller...\n");
    s_write_uninstaller();

    printf("\n  ========================\n");
    printf("  sparkd v%s installed successfully!\n", SPARKD_VERSION);
    printf("  Open a new terminal and run: sparkctl --help\n\n");

    return 0;
}

#else
#include <stdio.h>
int main(void)
{
    fprintf(stderr, "spark-install is Windows-only.\n");
    return 1;
}
#endif
