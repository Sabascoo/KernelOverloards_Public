#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("MISSION CONTROL\n");

    // 1. Run the Main Logic first to generate data
    printf("Load in the main file\n");
    #ifdef _WIN32
        // system() on Windows waits for the process to finish unless 'start' is used
        int logicStatus = system("main.exe"); 
    #else
        int logicStatus = system("./main");
    #endif

    if (logicStatus != 0) {
        printf("Error: Failed to generate data\n");
        return 1;
    }

    // 2. Now launch the GUI (using 'start' or '&' so the launcher can close)
    printf("Logic complete. Powering up GUI...\n");
    #ifdef _WIN32
        system("start gui.exe");
    #else
        system("./gui &");
    #endif

    printf("Mission Control active. Launcher closing.\n");
    return 0;
}