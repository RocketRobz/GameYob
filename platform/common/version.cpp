#include <stdio.h>
#include "console.h"

void printVersionInfo() {
    clearConsole();
    printf("GameYob-Accurate %s\n", VERSION_STRING);
}
