#ifndef modeManager_H
#define modeManager_H

using namespace std;

//#include "touchscreen.h" // This has the definition of the touch_t structure
#include "Menu.h" // Menu class


// Mode Management enumeration definition and application mode definitions
enum ApplicationMode {
						noMode,
						dashboardMode,
						plotMode,
						logMode,
						diagnosticMode,
						informationMode,
						interfaceMode,
						settingsMode,
						developmentMode
};



void modeManager (Menu*, touch_t*, ApplicationMode*, ApplicationMode*);



#endif