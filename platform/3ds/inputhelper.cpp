#include <3ds.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <string>

#include "inputhelper.h"
#include "gameboy.h"
#include "main.h"
#include "console.h"
#include "menu.h"
#include "nifi.h"
#include "gbgfx.h"
#include "soundengine.h"
#include "cheats.h"
#include "gbs.h"
#include "filechooser.h"
#include "romfile.h"
#include "io.h"
#include "gbmanager.h"

u32 lastKeysPressed = 0;
u32 keysPressed = 0;
u32 keysJustPressed = 0;

u32 keysForceReleased=0;
u32 repeatStartTimer=0;
int repeatTimer=0;

u8 buttonsPressed;

char biosPath[MAX_FILENAME_LEN] = "";
char borderPath[MAX_FILENAME_LEN] = "";
char romPath[MAX_FILENAME_LEN] = "";

bool fastForwardMode = false; // controlled by the toggle hotkey
bool fastForwardKey = false;  // only while its hotkey is pressed

bool biosExists = false;
int rumbleInserted = 0;


void initInput()
{
}

void flushFatCache() {
}


void controlsParseConfig(const char* line2) {
}
void controlsPrintConfig(FileHandle* file) {
}

void startKeyConfigChooser() {
}

void generalParseConfig(const char* line) {
    char* equalsPos;
    if ((equalsPos = (char*)strrchr(line, '=')) != 0 && equalsPos != line+strlen(line)-1) {
        *equalsPos = '\0';
        const char* parameter = line;
        const char* value = equalsPos+1;

        if (strcasecmp(parameter, "rompath") == 0) {
            strcpy(romPath, value);
            romChooserState.directory = std::string(romPath);
        }
        else if (strcasecmp(parameter, "biosfile") == 0) {
            strcpy(biosPath, value);
        }
        else if (strcasecmp(parameter, "borderfile") == 0) {
            strcpy(borderPath, value);
        }
    }
    if (*borderPath == '\0') {
        strcpy(borderPath, "/border.bmp");
    }
}

void generalPrintConfig(FileHandle* file) {
    file_printf(file, "rompath=%s\n", romPath);
    file_printf(file, "biosfile=%s\n", biosPath);
    file_printf(file, "borderfile=%s\n", borderPath);
}

bool readConfigFile() {
    FileHandle* file = file_open("/gameyob.ini", "r");
    char line[100];
    void (*configParser)(const char*) = generalParseConfig;

    if (file == NULL)
        goto end;

    while (file_tell(file) < file_getSize(file)) {
        file_gets(line, 100, file);
        char c=0;
        while (*line != '\0' && ((c = line[strlen(line)-1]) == ' ' || c == '\n' || c == '\r'))
            line[strlen(line)-1] = '\0';
        if (line[0] == '[') {
            char* endBrace;
            if ((endBrace = strrchr(line, ']')) != 0) {
                *endBrace = '\0';
                const char* section = line+1;
                if (strcasecmp(section, "general") == 0) {
                    configParser = generalParseConfig;
                }
                else if (strcasecmp(section, "console") == 0) {
                    configParser = menuParseConfig;
                }
                else if (strcasecmp(section, "controls") == 0) {
                    configParser = controlsParseConfig;
                }
            }
        }
        else
            configParser(line);
    }
    file_close(file);
end:

    return file != NULL;
}

void writeConfigFile() {
    FileHandle* file = file_open("/gameyob.ini", "w");
    if (file == NULL) {
        printMenuMessage("Error opening gameyob.ini.");
        return;
    }

    file_printf(file, "[general]\n");
    generalPrintConfig(file);
    file_printf(file, "[console]\n");
    menuPrintConfig(file);
    file_printf(file, "[controls]\n");
    controlsPrintConfig(file);
    file_close(file);

    char nameBuf[256];
    sprintf(nameBuf, "%s.cht", gameboy->getRomFile()->getBasename());
    gameboy->getCheatEngine()->saveCheats(nameBuf);
}


bool keyPressed(int key) {
    return keysPressed & key;
}
bool keyPressedAutoRepeat(int key) {
    if (keyJustPressed(key)) {
        repeatStartTimer = 14;
        return true;
    }
    if (keyPressed(key) && repeatStartTimer == 0 && repeatTimer == 0) {
        repeatTimer = 2;
        return true;
    }
    return false;
}
bool keyJustPressed(int key) {
    return keysJustPressed & key;
}

void forceReleaseKey(int key) {
    keysForceReleased |= key;
    keysPressed &= ~key;
}

int mapFuncKey(int funcKey) {
    switch(funcKey) {
        case FUNC_KEY_NONE:
            return 0;
        case FUNC_KEY_A:
            return KEY_A;
        case FUNC_KEY_B:
            return KEY_B;
        case FUNC_KEY_LEFT:
            return KEY_LEFT;
        case FUNC_KEY_RIGHT:
            return KEY_RIGHT;
        case FUNC_KEY_UP:
            return KEY_UP;
        case FUNC_KEY_DOWN:
            return KEY_DOWN;
        case FUNC_KEY_START:
            return KEY_START | KEY_X;
        case FUNC_KEY_SELECT:
            return KEY_SELECT | KEY_Y;
        case FUNC_KEY_MENU:
            return KEY_R | KEY_TOUCH;
        case FUNC_KEY_MENU_PAUSE:
            return 0;
        case FUNC_KEY_SAVE:
            return 0;
        case FUNC_KEY_AUTO_A:
            return 0;
        case FUNC_KEY_AUTO_B:
            return 0;
        case FUNC_KEY_FAST_FORWARD:
            return KEY_L;
        case FUNC_KEY_FAST_FORWARD_TOGGLE:
            return 0;
        case FUNC_KEY_SCALE:
            return 0;
        case FUNC_KEY_RESET:
            return 0;
        default:
            return 0;
    }
}

int mapMenuKey(int menuKey) {
    switch (menuKey) {
        case MENU_KEY_A:
            return KEY_A;
        case MENU_KEY_B:
            return KEY_B;
        case MENU_KEY_UP:
            return KEY_UP;
        case MENU_KEY_DOWN:
            return KEY_DOWN;
        case MENU_KEY_LEFT:
            return KEY_LEFT;
        case MENU_KEY_RIGHT:
            return KEY_RIGHT;
        case MENU_KEY_L:
            return KEY_L;
        case MENU_KEY_R:
            return KEY_R;
        default:
            return 0;
    }
}

void inputUpdateVBlank() {
    hidScanInput();
    lastKeysPressed = keysPressed;
    keysPressed = hidKeysHeld();

    for (int i=0; i<32; i++) {
        if (keysForceReleased & (1<<i)) {
            if (!(keysPressed & (1<<i)))
                keysForceReleased &= ~(1<<i);
        }
    }
    keysPressed &= ~keysForceReleased;

    keysJustPressed = (lastKeysPressed ^ keysPressed) & keysPressed;

    if (repeatTimer > 0)
        repeatTimer--;
    if (repeatStartTimer > 0)
        repeatStartTimer--;
}

void system_doRumble(bool rumbleVal)
{
}

int system_getMotionSensorX() {
    return 0;
}
int system_getMotionSensorY() {
    return 0;
}


void system_checkPolls() {
    APP_STATUS status;

	while((status=aptGetStatus()) != APP_RUNNING) {

        if(status == APP_SUSPENDING)
        {
            aptReturnToMenu();
        }
        else if(status == APP_PREPARE_SLEEPMODE)
        {
			aptSignalReadyForSleep();
            aptWaitStatusEvent();
        }
        else if (status == APP_SLEEPMODE) {

        }
        else if (status == APP_EXITING) {
            mgr_save();
            mgr_exit();

            fsExit();
            gfxExit();
            hidExit();
            aptExit();
            srvExit();

            exit(0);
        }

        gspWaitForVBlank();
    }

    gfxFlushBuffers();
    gfxSwapBuffersGpu();
}

void system_waitForVBlank() {
    gspWaitForVBlank();
}