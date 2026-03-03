#include <stdio.h>
#include <stdlib.h>

#define BUILD_DIR "build-mingw"

#ifdef _WIN32
  #include <windows.h>
  #include <shellapi.h>

  #define MAIN_EXE  "build-mingw\\main_logic.exe"
  #define GUI_EXE   "build-mingw\\gui.exe"
#else
  #include <unistd.h>
  #include <sys/types.h>

  #define MAIN_EXE  "./build/main_logic"
  #define GUI_EXE   "./build/gui"
#endif

static int run_cmd(const char *cmd) {
    printf(">> %s\n", cmd);
    int rc = system(cmd);
    if (rc != 0) printf("!! Command failed (%d): %s\n", rc, cmd);
    return rc;
}

static int spawn_gui_detached(void) {
#ifdef _WIN32
    HINSTANCE r = ShellExecuteA(NULL, "open", GUI_EXE, NULL, NULL, SW_SHOWNORMAL);

    // Per docs: > 32 means success, <= 32 is error code
    if ((INT_PTR)r <= 32) {
        printf("ShellExecute failed, code=%ld\n", (long)(INT_PTR)r);
        return 1;
    }
    return 0;
#else
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        setsid(); // detach
        execl(GUI_EXE, GUI_EXE, (char*)NULL);
        _exit(1);
    }
    return 0;
#endif
}

int main(void) {
    printf("MISSION CONTROL LAUNCHER\n");

#ifdef _WIN32
    if (run_cmd("cmake -S . -B " BUILD_DIR " -G \"MinGW Makefiles\" -DCMAKE_BUILD_TYPE=Release") != 0) return 1;
    if (run_cmd("cmake --build " BUILD_DIR) != 0) return 1;
#else
    if (run_cmd("cmake -S . -B build -DCMAKE_BUILD_TYPE=Release") != 0) return 1;
    if (run_cmd("cmake --build build -j") != 0) return 1;
#endif

    printf("Running main logic...\n");
    if (run_cmd(MAIN_EXE) != 0) return 1;

    printf("Launching GUI...\n");
    if (spawn_gui_detached() != 0) {
        printf("Error: Failed to launch GUI.\n");
        return 1;
    }

    printf("Launcher exiting.\n");
    return 0;
}