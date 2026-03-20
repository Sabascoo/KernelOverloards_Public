#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
    #include <shellapi.h>
    #include <sys/stat.h>

    #define BUILD_DIR "build-mingw"
    #define MAIN_EXE  BUILD_DIR "\\main_logic.exe"
    #define GUI_EXE   BUILD_DIR "\\gui.exe"
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <sys/stat.h>

    #define BUILD_DIR "build"
    #define MAIN_EXE  "./build/main_logic"
    #define GUI_EXE   "./build/gui"
#endif

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int run_cmd(const char *cmd) {
    printf(">> %s\n", cmd);
    int rc = system(cmd);
    if (rc != 0) {
        printf("!! Command failed (%d): %s\n", rc, cmd);
        return 1;
    }
    return 0;
}

static int ensure_built(void) {
    if (file_exists(MAIN_EXE) && file_exists(GUI_EXE)) {
        printf("Build artifacts already exist. Skipping rebuild.\n");
        return 0;
    }

    printf("Build artifacts missing. Configuring and building...\n");

#ifdef _WIN32
    if (run_cmd("cmake -S . -B " BUILD_DIR " -G \"MinGW Makefiles\" -DCMAKE_BUILD_TYPE=Release") != 0)
        return 1;
    if (run_cmd("cmake --build " BUILD_DIR " --config Release") != 0)
        return 1;
#else
    if (run_cmd("cmake -S . -B " BUILD_DIR " -DCMAKE_BUILD_TYPE=Release") != 0)
        return 1;
    if (run_cmd("cmake --build " BUILD_DIR " -j") != 0)
        return 1;
#endif

    if (!file_exists(MAIN_EXE) || !file_exists(GUI_EXE)) {
        printf("!! Build finished, but expected executables were not found.\n");
        return 1;
    }

    return 0;
}

static int run_main_logic(void) {
    printf("Running main logic...\n");

#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmdline[] = MAIN_EXE;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(
            NULL,           // app name
            cmdline,        // command line
            NULL, NULL,     // process/thread security
            FALSE,          // inherit handles
            0,              // creation flags
            NULL, NULL,     // environment/current dir
            &si, &pi)) {
        printf("CreateProcess failed for main_logic (error=%lu)\n", GetLastError());
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 1;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        printf("GetExitCodeProcess failed (error=%lu)\n", GetLastError());
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (exit_code != 0) {
        printf("main_logic exited with code %lu\n", (unsigned long)exit_code);
        return 1;
    }

    return 0;
#else
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execl(MAIN_EXE, MAIN_EXE, (char *)NULL);
        perror("execl main_logic");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("main_logic failed.\n");
        return 1;
    }

    return 0;
#endif
}

static int spawn_gui_detached(void) {
    printf("Launching GUI...\n");

#ifdef _WIN32
    HINSTANCE r = ShellExecuteA(NULL, "open", GUI_EXE, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) {
        printf("ShellExecute failed, code=%ld\n", (long)(INT_PTR)r);
        return 1;
    }
    return 0;
#else
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        setsid();  // detach from terminal/session
        execl(GUI_EXE, GUI_EXE, (char *)NULL);
        perror("execl gui");
        _exit(127);
    }

    return 0;
#endif
}

int main(void) {
    printf("MISSION CONTROL LAUNCHER\n");

    if (ensure_built() != 0) {
        printf("Error: build step failed.\n");
        printf("Checking for %s\n", MAIN_EXE);
        printf("Checking for %s\n", GUI_EXE);
        return 1;
    }

    if (run_main_logic() != 0) {
        printf("Error: main logic failed.\n");
        return 1;
    }

    if (spawn_gui_detached() != 0) {
        printf("Error: failed to launch GUI.\n");
        return 1;
    }

    printf("Launcher exiting.\n");
    return 0;
}