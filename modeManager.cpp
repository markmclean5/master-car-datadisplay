#include "modeManager.h"


// Mode management function
void modeManager (Menu* ModeMenuPtr, touch_t* menuTouch, ApplicationMode* previousMode, ApplicationMode* currentMode) {
	// Default mode upon initialization
	if(*currentMode == noMode) {
		*currentMode = developmentMode;
		ModeMenuPtr->selectButton("development");
	}
	ModeMenuPtr->update(menuTouch);
	string btnPressedString = ModeMenuPtr->getPressedButtonName();
	string prevModeString = ModeMenuPtr->getSelectedButtonName();

	// Change modes
	if(!btnPressedString.empty() && btnPressedString.compare(prevModeString) != 0) {
		//previousMode = currentMode;
		ModeMenuPtr->selectButton(btnPressedString);
		if(ModeMenuPtr->isButtonSelected("dashboard")) *currentMode = dashboardMode;
		if(ModeMenuPtr->isButtonSelected("plot")) *currentMode = plotMode;
		if(ModeMenuPtr->isButtonSelected("log")) *currentMode = logMode;
		if(ModeMenuPtr->isButtonSelected("diagnostic")) *currentMode = diagnosticMode;
		if(ModeMenuPtr->isButtonSelected("information")) *currentMode = informationMode;
		if(ModeMenuPtr->isButtonSelected("interface")) *currentMode = interfaceMode;
		if(ModeMenuPtr->isButtonSelected("settings")) *currentMode = settingsMode;
		if(ModeMenuPtr->isButtonSelected("development")) *currentMode = developmentMode;
		if(ModeMenuPtr->isButtonSelected("exit")) {
			ioctl(threadTouch.fd, EVIOCGRAB, 0); // Release touch for this application
			exit(EXIT_SUCCESS);
		}
	}
	else *previousMode = *currentMode;

}