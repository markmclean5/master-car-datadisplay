using namespace std;
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include "VG/openvg.h"		//
#include "VG/vgu.h"		//
#include "EGL/egl.h"		// EGL for pbuffers
#include "../openvg/fontinfo.h"	// Openvg
#include "../openvg/shapes.h"	//
#include <stdlib.h>		//
#include "Gauge.h"		// Gauge class
#include "DataStream.h"		// DataStream class
#include <linux/input.h>
#include <fcntl.h>
#include <pthread.h>
#include "avengeance.inc"	// *** Label Fonts
#include <bcm2835.h>
#include <vector>
#include <time.h>
#include <fstream>			// log file

// Project Classes & utilities
#include "touchscreen.h"
#include "main.h"
#include "Button.h"
#include "TextView.h"
#include "Menu.h"
#include "Serial.h"
#include "PID.h"
#include <algorithm>


// Loop time
uint64_t loopTime;
touch_t threadTouch, loopTouch;
int quitState = 0;

VGImage BackgroundImage;

Menu *ModeMenuPtr;
Serial *ELMSerialPtr;

// Prototypes
void setupGraphics(int*,int*);
void DisplayObjectManager(std::vector<Button>&, std::vector<Gauge>&, std::vector<PID>&, std::vector<Menu>&);
void modeManager (touch_t*);
void PIDVectorManager(void);


// PID Vector update rate times
uint64_t lastPIDVectorUpdateTime = 0;
uint64_t secondLastPIDVectorUpdateTime = 0;


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

ApplicationMode currentMode = noMode;			// Current application mode
ApplicationMode previousMode = noMode;			// Previous application mode, for mode change detection

// PID Vector management enumeration definition
enum PIDVectorState {
						inactive,		// Active PID updates not required
						active,			// Active, not waiting on response from ELM
						busy,			// Waiting for response from ELM
						complete		// All PIDs in vector have been updated
};


// Connection Status Enums
enum ConnectionStatus { disconnected, connecting, connected };

enum ParmStatus { unknown, requested, known };

//Dashboard mode vectors and states
vector<Button> DASHBOARD_HotButtons;
vector<Gauge> DASHBOARD_Gauges;
vector<Menu> DASHBOARD_Menus;



// Everything to do with PIDs
vector<PID> PIDs;										// PID vector
std::vector<PID>::iterator CurrentPID = PIDs.begin();	// Set up iterator for PID vector
PIDVectorState PIDVectorState = inactive;				// PID vector state for processing states
int numPIDs = 0;										// PID counter for vector size change detection


// TODO: Figure out a better way to deal with colors
// Color definitions (float r, float g, float b, float alpha)
float black[] = {0,0,0,1};
float green[] = {0,1,0,1};
float red[] = {1,0,0,1};
float blue[] = {0,0,1,1};
float white[] = {1,1,1,1};
float translucentBlack[] = {0,0,0,0.5};
float gray[] = {0.43,0.43,0.43,1};
float sendcolor[] = {1.0, 0.4, 0.4, 1.0};
float receivecolor[] = {0.4, 0.69, 1.0, 1.0};






/**********************************************************************************************************************************************************
																														M A I N    E X E C U T I O N    L O O P
**********************************************************************************************************************************************************/


int main() {
	int width, height;						// display width & height
	setupGraphics(&width, &height);			// Initialize display

	if (!bcm2835_init())					// Initialize bcm2835 library
		return 1;
	if (touchinit(width, height) != 0) {	// Initialize touchscreen
		
		fprintf(stderr, "Unable to initialize touchscreen\n");
		exit(1);
	}
	

	
	//end testing purposes

	// Vector size reservations, based on expected application usage, to minimize resize operations
	PIDs.reserve(4);
	DASHBOARD_Menus.reserve(4);
	DASHBOARD_HotButtons.reserve(4);
	DASHBOARD_Gauges.reserve(4);

	// Draw background wallpaper from JPEG, save as VGImage for efficient re-draw
	//Image(0, 0, 800, 480, "/home/pi/master-car-datadisplay/wallpaper.jpg");
	Image(0, 0, 800, 480, "/home/pi/master-car-datadisplay/pats.jpg");
	BackgroundImage = vgCreateImage(VG_sABGR_8888, 800, 480, VG_IMAGE_QUALITY_BETTER);
	vgGetPixels(BackgroundImage, 0, 0, 0, 0, 800, 480);
	

	// Mode Menu: managed by mode menu manager
	Menu ModeMenu(width/2, height-30, width-20, 50, "ModeMenu");
	ModeMenuPtr = &ModeMenu;
	ModeMenu.touchEnable();

	// Serial object for ELM327 interface
	Serial ELMSerial("/dev/ELM327", B38400);
	ELMSerialPtr = &ELMSerial;
	ELMSerial.setEndChar('>');
	ELMSerial.setReadTimeout(5000);		// TODO: Investigate me


	// Dashboard Mode items
	int numPIDs = 0;
	bool waitingOnData = false;

	int gaugeCenterX = width/2;
	int gaugeCenterY = height/2-30;
	int gaugeRadius = height/2 - 80;

	
	// Bottom of display status readouts
	Button FramerateButton(25, 18, 50, 35, "FramerateButton");
	Button PIDUpdateRateButton(80, 18, 50, 35, "FramerateButton");
	
	Button ELMConnectionStatusButton(200, 18, 100, 35, "ConnectionButton");
	Button ECUConnectionStatusButton(350, 18, 160, 35, "ConnectionButton");
	
	// ELM connection and ECU connection status
	ConnectionStatus 	ELMStatus 					= disconnected;
	uint64_t 					ELMConnectStartTime 	= 0;
	int 							ELMConnectTimeout 	= 1000; 					// Timeout, milliseconds
	string 						ELMResponseString 	= "";
	string 						ELMVersion 					= "";
	ParmStatus 				ELMVersionStat 			= unknown;
	string 						Protocol 						= "";
	ParmStatus 				ProtocolStat 				= unknown;
	ConnectionStatus 	ECUStatus 					= disconnected;
	int 							ECUConnectTries 		= 0;
	string 						ECUResponseString 	= "";
	string 						VIN 								= "";
	ParmStatus 				VINStat 						= unknown;
	
	uint64_t lastLoopTime = 0;
	
	
	// PID support 

	Menu SupportedPIDMenu(width/2, height/2, width/3-20, 360, "SupportedPIDMenu");
			
	vector<PID> SupportPIDs;			// PID support queries
	
	ParmStatus PIDSupportRequestStatus = unknown;
	int currentPIDSupportRequest = 100;


	// Log Mode

	bool logging = false;

	Button NewLogButton(width/8, height-80, width/4 - 20, 40, "NewLogButton");

	TextView LogParameterView(width/8, height-185, width/4 - 20, 160, "LogParameterView");
	LogParameterView.addNewLineIdentifier("FilenameLabel", "Filename:");
	LogParameterView.setLineIdentifierText("FilenameLabel", "");
	LogParameterView.addNewLineIdentifier("Filename", "");
	LogParameterView.setLineIdentifierText("Filename", "-----------");
	LogParameterView.addNewLineIdentifier("EntriesLabel", "Entries:");
	LogParameterView.setLineIdentifierText("EntriesLabel", "");
	LogParameterView.addNewLineIdentifier("Entries", "");
	LogParameterView.setLineIdentifierText("Entries", "-----");

	LogParameterView.addNewLineIdentifier("PIDsLabel", "PIDs:");
	LogParameterView.setLineIdentifierText("PIDsLabel", "");
	LogParameterView.addNewLineIdentifier("PIDs", "");
	LogParameterView.setLineIdentifierText("PIDs", "--");

	LogParameterView.addNewLineIdentifier("UpdateRateLabel", "Rate:");
	LogParameterView.setLineIdentifierText("UpdateRateLabel", "");
	LogParameterView.addNewLineIdentifier("UpdateRate", "");
	LogParameterView.setLineIdentifierText("UpdateRate", "-- Hz");


	Button LogStateControlButton(width/8, height - 290, width/4 - 20, 40, "LogStateControlButton");
	LogStateControlButton.touchEnable();


	string logTimeString = "Time String ";
	time_t t = time(0);

	struct tm * time_tm = localtime (&t);

	logTimeString.append(std::to_string(time_tm->tm_mon+1));


	cout << logTimeString << endl;

 	cout << (time_tm->tm_mon + 1) << '-'
         <<  time_tm->tm_mday << '-'
         << (time_tm->tm_year + 1900)
         << endl;


     // LOG FILE
     ofstream logfile;
     string logPath = "/tmp/carlogs/";
     string logFileName;


	 
	 
	 
/**********************************************************************************************************************************************************
																														M A I N    W H I L E    L O O P
**********************************************************************************************************************************************************/
	
	while(1) {
		
		// **** Global Loop  Parameters ****//
		
		loopTouch = threadTouch;											// Get touch for loop
		loopTime = bcm2835_st_read();										// Get time for loop
		//serialData = ELMSerial.serialReadUntil();							// Get serial data for loop
		vgSetPixels(0, 0, BackgroundImage, 0, 0, 800, 480);					// Draw background image
		ModeMenu.update(&loopTouch);										// Update mode menu

	
	// Autoconnection and connection statusing logic	
	if(ELMStatus == disconnected) {
		ELMConnectionStatusButton.setText("ELM Connecting");
		ELMStatus = connecting;
		ELMSerial.serialWrite("ATZ"); //ELM reset
		ELMConnectStartTime = loopTime;		
	}
	if(ELMStatus == connecting && (loopTime < (ELMConnectStartTime + ELMConnectTimeout*1000))) {
		ELMResponseString = ELMSerial.serialReadUntil();
			if(!ELMResponseString.empty()) {
				ELMStatus = connected;
				ELMConnectionStatusButton.setText("ELM Connected");
					string resp = "";
					ELMSerial.serialWrite("ATE0"); // set elm327 output for NO ECHO
					while(resp.empty()) {
						resp = ELMSerial.serialReadUntil();
					}
					resp = "";
					ELMSerial.serialWrite("ATSP0");	// set ELM327 to automatically detect protocol
					while(resp.empty()) {
						resp = ELMSerial.serialReadUntil();
					}
			}
	}
	// ELM327 connection timeout
	if(ELMStatus == connecting && (loopTime >= (ELMConnectStartTime + ELMConnectTimeout*1000))) {		
		ELMStatus = disconnected;
		ELMConnectionStatusButton.setText("ELM Timeout");
	}
	// Reqest ELM327 Version information
	if(ELMStatus == connected && ELMVersionStat == unknown) {			
		ELMSerial.serialWrite("ATI");
		ELMVersionStat = requested;
	}

	// Requesting ELM327 Version information
	if(ELMVersionStat == requested) {
		ELMVersion = ELMSerial.serialReadUntil();
		if(!ELMVersion.empty()){
			ELMVersionStat = known; 
			ELMConnectionStatusButton.setText(ELMVersion);
		}
	}
	
	// Connect to the ECU after recieving ELM Version information
	if(ELMVersionStat == known && ECUStatus == disconnected) {
		ECUStatus = connecting;
		ELMSerial.serialWrite("0101");
	}
	
	// Connecting to the ECU
	if(ECUStatus == connecting ) {
		ECUResponseString = ELMSerial.serialReadUntil();
		if(!ECUResponseString.empty()) {
			cout << ECUResponseString << endl;
			ECUConnectTries++;
			string txt = "ECU Connecting - ";
			cout<<txt<<endl;
			txt.append(std::to_string(ECUConnectTries));
			cout<<txt<<endl;
			ECUConnectionStatusButton.setText(txt);
			if(ECUResponseString.find("41 01") != string::npos) {
				ECUStatus = connected;
			}
			else {
				ECUStatus = disconnected;
				ECUResponseString = "";
			}
		}

	}

	// Request current connection protocol information after connecting to the ECU
	if(ECUStatus == connected && ProtocolStat == unknown){
		ELMSerial.serialWrite("ATDP");
		ProtocolStat = requested;
	}

	// Requesting protocol information
	if(ProtocolStat == requested) {
		Protocol = ELMSerial.serialReadUntil();
		if(!Protocol.empty()){
			ProtocolStat = known; 
			ECUConnectionStatusButton.setText(Protocol);
		}

	}
	
	// Requestiing PID support from ECU (assuming protocol known)
	if(ProtocolStat == known && PIDSupportRequestStatus == unknown) {
		string requestPIDString = "0";
		requestPIDString.append(std::to_string(currentPIDSupportRequest) );	//"0100" first time around
		SupportPIDs.emplace_back(requestPIDString);
		ELMSerial.serialWrite(SupportPIDs.back().getCommand());
		PIDSupportRequestStatus = requested;
	}
	
	// Waiting on some PID support request response
	if(PIDSupportRequestStatus == requested) {
		string resp = ELMSerial.serialReadUntil();
		if(!resp.empty()) {
			SupportPIDs.back().update(resp, loopTime);
		}
		
		// Move on to next PID support request
		if(SupportPIDs.back().getBitPositionValue(31) && currentPIDSupportRequest < 160) {
			PIDSupportRequestStatus = unknown;
			currentPIDSupportRequest += 20;
		}
		// Or stop here
		else
			PIDSupportRequestStatus = known;
	
	}

	if(PIDSupportRequestStatus == known) {
		
		int numSupportPIDs = SupportPIDs.size();
		int PIDidx = 0;
		bool* PIDSupportStates = new bool[numSupportPIDs*31];
		string* PIDSupportNames = new string[numSupportPIDs*31];		
		
		
		for(std::vector<PID>::iterator it = SupportPIDs.begin(); it != SupportPIDs.end(); it++)  {
			for(int p = 0; p<30; p++) {
				PIDSupportStates[PIDidx] = (it)->getBitPositionValue(p);
				PIDSupportNames[PIDidx] = (it)->getBitPositionName(p);
					
				if(PIDSupportStates[PIDidx])
				cout << "Supported PID " << PIDSupportNames[PIDidx] << endl;
				SupportedPIDMenu.addItem(PIDSupportNames[PIDidx], PIDSupportNames[PIDidx]);
				PIDidx++;
			}
		}
	}
	
	
	
	
		
		// TODO: decide if this needs touch and should update the menu - if so remove double update
		modeManager(&loopTouch);

		
		// Framerate button - need to enable / disable
		float refreshRate = 1000000/(loopTime - lastLoopTime);
		lastLoopTime = loopTime;
		FramerateButton.setValue(refreshRate);
		FramerateButton.update();

		// PID Update rate button
		float PIDUpdateRate;

		if(lastPIDVectorUpdateTime != secondLastPIDVectorUpdateTime)
			PIDUpdateRate = 1000000/(lastPIDVectorUpdateTime - secondLastPIDVectorUpdateTime);
		else PIDUpdateRate = 0;

		PIDUpdateRateButton.setValue(PIDUpdateRate);
		PIDUpdateRateButton.update();

		// Connection status button
		ELMConnectionStatusButton.update();
		ECUConnectionStatusButton.update();
		
		
		

		
		
		PIDVectorManager();
		if (PIDVectorState == complete) PIDVectorState = active;	// Loop PID vector upon update completion
		
		
		
		
		/******************************************************************************************
		Mode 1 - DASHBOARD
		******************************************************************************************/
		// Dashboard mode run-time
		if(currentMode == dashboardMode) {
			for (std::vector<Button>::iterator it = DASHBOARD_HotButtons.begin(); it != DASHBOARD_HotButtons.end(); it++) {		// Update all hot buttons
				(it)->updateTouch(&loopTouch);
				(it)->update();
			}
			for (std::vector<Gauge>::iterator it = DASHBOARD_Gauges.begin(); it != DASHBOARD_Gauges.end(); it++) {				// Update all gauges
				(it)->updateTouch(&loopTouch);
				string name = (it)->getPIDLink();
				if(it->isTouched()) cout << "Gauge " << name << " is touched!" << endl;
				auto CurrentGaugePID_It = find_if(PIDs.begin(), PIDs.end(), [&name](PID& obj) {return obj.getCommand().compare(name) == 0;});
				// TODO - Understand and fix
				if(CurrentGaugePID_It != PIDs.end())
					(it)->update((CurrentGaugePID_It)->getRawDatum(), (CurrentGaugePID_It)->getEngUnits());
			}
			for (std::vector<Menu>::iterator it = DASHBOARD_Menus.begin(); it != DASHBOARD_Menus.end(); it++) {					// Update all menus
				(it)->update(&loopTouch);
			}
			
			DisplayObjectManager(DASHBOARD_HotButtons, DASHBOARD_Gauges, PIDs, DASHBOARD_Menus);								// Run DisplayObjectManager (current page dashboard hotbuttons, display objects, and PIDs)		
		} // End Mode 1

		
		
		/******************************************************************************************
		Mode 2 - PLOT
		******************************************************************************************/
		// Plot mode run-time
		if(currentMode == plotMode) {
			
		} // End Mode 2

		
		
		
		/******************************************************************************************
		Mode 3 - LOG
		******************************************************************************************/
		// Plot mode run-time
		if(currentMode == logMode) {
			NewLogButton.update();
			LogParameterView.update();
			LogStateControlButton.updateTouch(&loopTouch);
			LogStateControlButton.update();

			if(LogStateControlButton.isPressed()) {
				if(!logging) {			// Start logging
					logging = true;
					
					time_t t = time(0);

					struct tm * time_tm = localtime (&t);

					string logTimeString = std::to_string(time_tm->tm_mon+1);
					logTimeString.append("_");
					logTimeString.append(std::to_string(time_tm->tm_mday));
					logTimeString.append("_");
					logTimeString.append(std::to_string(time_tm->tm_year + 1900));
					logTimeString.append("_");
					logTimeString.append(std::to_string(time_tm->tm_hour));
					logTimeString.append("_");
					logTimeString.append(std::to_string(time_tm->tm_min));
					logTimeString.append("_");
					logTimeString.append(std::to_string(time_tm->tm_sec));

					cout << "Log Time String: " << logTimeString << endl;
					LogParameterView.setLineIdentifierText("Filename", logTimeString);

					logFileName = "../";
					logFileName.append(logTimeString);

					logfile.open(logFileName);
					logfile << "TEST";
					logfile.close();

				}
				else {
					logging = false;	// Stop logging
					LogParameterView.setLineIdentifierText("Filename", "-----------");
				}

				if(logging) {
					LogStateControlButton.select();
					LogStateControlButton.setText("Stop");
				}
				else {
					LogStateControlButton.deselect();
					LogStateControlButton.setText("Start");

				}
			}
		} // End Mode 3
		
		
		
		/******************************************************************************************
		Mode 8 - DEVELOPMENT
		******************************************************************************************/
		
		if(currentMode == developmentMode) {


		SupportedPIDMenu.update(&loopTouch);
		
		
		} // End Mode 8





		End();				// Write picture to screen




		

	}  // E N D    M A I N    W H I L E
}  // E N D    M A I N    L O O P










/**********************************************************************************************************************************************************
																								COMPONENT FUNCTIONS {TO BECOME SEPERATE CPP FILES}
**********************************************************************************************************************************************************/


// PID Vector managment function: Handles update of PID vectors based on current application mode
void PIDVectorManager (void) {

	if(PIDVectorState == inactive && (PIDs.size() != 0)) {				// Initial vector activation
		PIDVectorState = active;
		//cout << "Activating PID Vector" << endl;
		
		
	}

	
	if(PIDs.size() == 0 && PIDVectorState == active){
		
		PIDVectorState = inactive;
		//cout<< "Dactivating PID Vector" <<endl;
	}

	
	if(PIDVectorState != inactive && PIDVectorState != complete) {
		if(numPIDs != PIDs.size()) {
			if(PIDVectorState != busy) {								// Reset PID vector beginning iterator if vector modified
				numPIDs = PIDs.size();		
				CurrentPID = PIDs.begin();
				//cout << "PID Vector modified - resetting iterator to start" << endl; 	
			}
			else {														// Discard requested data if vector modified
				string dump = ELMSerialPtr->serialReadUntil();
				if(!dump.empty()) PIDVectorState = active;
			}
		}
		else if(PIDVectorState == active) {
			//cout << "PID Vector active" << endl;
			if(		(currentMode == dashboardMode && CurrentPID->dashboard_datalinks != 0) ||
					(currentMode == plotMode && CurrentPID->plot_datalinks != 0) ||
					(currentMode == logMode && CurrentPID->log_datalinks != 0)) {
				//cout << "Requirements met to update PID - setting vector busy" << endl;
				ELMSerialPtr->serialWrite((CurrentPID)->getCommand());		// Request PID data if necessary
				PIDVectorState = busy;
			}
			else {
				//cout << "Requirements not net to update PID - moving to next" << endl;
				CurrentPID++;		// Move on to next PID
			}
		}

			
		}
		if(PIDVectorState == busy) {								// Read PID response
			string serialResponse = ELMSerialPtr->serialReadUntil();
			if(!serialResponse.empty()) {
				cout << "Data recieved! updating PID with" << serialResponse << endl;
				(CurrentPID)->update(serialResponse, loopTime);		
				if(CurrentPID < PIDs.end()) {
					CurrentPID++;										// On to the next one
					PIDVectorState = active;
			}	
		}
		if(CurrentPID == PIDs.end()) {
			cout << "PID Vector update complete." << endl;
			PIDVectorState = complete;							// All PIDs updated
			CurrentPID = PIDs.begin();

			// Capture vector update completion time
			secondLastPIDVectorUpdateTime = lastPIDVectorUpdateTime;
			lastPIDVectorUpdateTime = bcm2835_st_read();
		} 
					
	}
}





// Mode management function
void modeManager (touch_t* menuTouch) {
	// Default mode upon initialization
	if(currentMode == noMode) {
		currentMode = dashboardMode;
		ModeMenuPtr->selectButton("dashboard");
	}
	ModeMenuPtr->update(menuTouch);
	string btnPressedString = ModeMenuPtr->getPressedButtonName();
	string prevModeString = ModeMenuPtr->getSelectedButtonName();

	// Change modes
	if(!btnPressedString.empty() && btnPressedString.compare(prevModeString) != 0) {
		//previousMode = currentMode;
		ModeMenuPtr->selectButton(btnPressedString);
		if(ModeMenuPtr->isButtonSelected("dashboard")) currentMode = dashboardMode;
		if(ModeMenuPtr->isButtonSelected("plot")) currentMode = plotMode;
		if(ModeMenuPtr->isButtonSelected("log")) currentMode = logMode;
		if(ModeMenuPtr->isButtonSelected("diagnostic")) currentMode = diagnosticMode;
		if(ModeMenuPtr->isButtonSelected("information")) currentMode = informationMode;
		if(ModeMenuPtr->isButtonSelected("interface")) currentMode = interfaceMode;
		if(ModeMenuPtr->isButtonSelected("settings")) currentMode = settingsMode;
		if(ModeMenuPtr->isButtonSelected("development")) currentMode = developmentMode;
		if(ModeMenuPtr->isButtonSelected("exit")) {
			ioctl(threadTouch.fd, EVIOCGRAB, 0); // Release touch for this application
			exit(EXIT_SUCCESS);
		}
	}
	else previousMode = currentMode;

}

void DisplayObjectManager(std::vector<Button>& HotButtons, std::vector<Gauge>& Gauges, std::vector<PID>& PIDs, std::vector<Menu>& Menus){

	int width = 800;
	int height = 480;
	// Create & touch enable hotbuttons if no buttons or gauges exist
	if(HotButtons.size() == 0 && Gauges.size() == 0) {
		HotButtons.emplace_back(800/6, (480/2) - 30, 80, 80, "hotBtn1");
		HotButtons.back().touchEnable();
		HotButtons.emplace_back(800/2, (480/2) - 30, 80, 80, "hotBtn2");
		HotButtons.back().touchEnable();
		HotButtons.emplace_back(800 - 800/6, (480/2) - 30, 80, 80, "hotBtn3");
		HotButtons.back().touchEnable();
	}


	std::vector<Button>::iterator selectedHotbutton_It = HotButtons.end();

	// Handle all hot button logic here
	for (std::vector<Button>::iterator currentHotButton = HotButtons.begin(); currentHotButton != HotButtons.end(); currentHotButton++) {
		// When hotbutton is pressed, disable touch on hotbuttons & display objects
		// Also create parameter selection menus
		if(currentHotButton->isPressed()) {
			currentHotButton->select();
			for (std::vector<Button>::iterator hb = HotButtons.begin(); hb != HotButtons.end(); hb++)
				hb->touchDisable();
			for (std::vector<Gauge>::iterator g = Gauges.begin(); g != Gauges.end(); g++)
				g->touchDisable();
			Menus.emplace_back(width/6, (height/2)-30, (width/3) - 10, height-70, "HOTBUTTON_GroupMenu");
			string defaultGroup = "g1";
			Menus.back().selectButton(defaultGroup);
			Menus.emplace_back(width-width/6, (height/2)-30, (width/3)- 10, height-70, "HOTBUTTON_DisplayObjectMenu");
			cout << "default group: " << defaultGroup << endl;
			Menus.emplace_back(width/2, (height/2)-30, (width/3)- 10, height-70, defaultGroup);
		}

		// If hotbutton is selected - set selected button iterator
		if(currentHotButton->isSelected()) selectedHotbutton_It = currentHotButton;
	}

	// Create menus if menus are not present
	if(Menus.size() != 0) {

		string name = "HOTBUTTON_GroupMenu";
		auto HOTBUTTON_GroupMenu_It = find_if(Menus.begin(), Menus.end(), [&name](const Menu& obj) {return obj.menuIdentifier.compare(name) == 0;});
		name = "HOTBUTTON_DisplayObjectMenu";
		auto HOTBUTTON_DisplayObjectMenu_It = find_if(Menus.begin(), Menus.end(), [&name](const Menu& obj) {return obj.menuIdentifier.compare(name) == 0;});
		string type = "ParameterMenu";
		auto HOTBUTTON_ParameterMenu_It = find_if(Menus.begin(), Menus.end(), [&type](const Menu& obj) {return obj.menuType.compare(type) == 0;});


		for (std::vector<Menu>::iterator currentMenu = Menus.begin(); currentMenu != Menus.end(); currentMenu++) {
			string pressedButtonName = currentMenu->getPressedButtonName();
			if(!pressedButtonName.empty()) currentMenu->selectButton(pressedButtonName);
		}
		
		if(HOTBUTTON_GroupMenu_It->getSelectedButtonName().compare(HOTBUTTON_ParameterMenu_It->menuIdentifier) != 0) {
			cout << "changing parameter menu to: "<<  HOTBUTTON_GroupMenu_It->getSelectedButtonName() << endl; 
			Menus.erase(HOTBUTTON_ParameterMenu_It);
			Menus.emplace_back(width/2, (height/2)-30, (width/3)- 10, height-70, HOTBUTTON_GroupMenu_It->getSelectedButtonName());
			type = HOTBUTTON_GroupMenu_It->getSelectedButtonName();
			HOTBUTTON_ParameterMenu_It = find_if(Menus.begin(), Menus.end(), [&type](const Menu& obj) {return obj.menuType.compare(type) == 0;});
		}

		


		// All selections made, add item and PID
		if(	!HOTBUTTON_GroupMenu_It->getSelectedButtonName().empty()
			&& !HOTBUTTON_ParameterMenu_It->getSelectedButtonName().empty()
			&& !HOTBUTTON_DisplayObjectMenu_It->getSelectedButtonName().empty()) {

			cout << "selected group: " << HOTBUTTON_GroupMenu_It->getSelectedButtonName() << endl;
			cout << "selected parameter: " << HOTBUTTON_ParameterMenu_It->getSelectedButtonName() << endl;
			cout << "selected display type:" << HOTBUTTON_DisplayObjectMenu_It->getSelectedButtonName() << endl;

			if(HOTBUTTON_DisplayObjectMenu_It->getSelectedButtonName().compare("Gauge") == 0) {
				cout << "creating a gauge!!" << endl;

				std::vector<PID>::iterator p = PIDs.begin();
				for (;p != PIDs.end(); p++){
					if(p->getCommand().compare(HOTBUTTON_ParameterMenu_It->getSelectedButtonName())==0) {
						p->dashboard_datalinks++;
						cout << "PID exists in vector - adding dashboard datalink" << endl;
						break;
					}
				}

				if(p == PIDs.end()) {
					PIDs.emplace_back(HOTBUTTON_ParameterMenu_It->getSelectedButtonName());
					PIDs.back().dashboard_datalinks++;
					cout << "Added new PID to vector and added dashboard datalink" << endl;
				}
				
				Gauges.emplace_back(selectedHotbutton_It->getDOPosX(), selectedHotbutton_It->getDOPosY(), width/6, HOTBUTTON_ParameterMenu_It->getSelectedButtonName().append("Gauge"));
				Gauges.back().draw();
				Gauges.back().touchEnable();
				// Erase menus in reverse creation order to prevent invalid iterators		
				Menus.erase(HOTBUTTON_ParameterMenu_It);
				Menus.erase(HOTBUTTON_DisplayObjectMenu_It);
				Menus.erase(HOTBUTTON_GroupMenu_It);
				
				// Hide the selected hot button and re-enable touch on all other visible buttons
				for (std::vector<Button>::iterator currentHotButton = HotButtons.begin(); currentHotButton != HotButtons.end(); currentHotButton++) {
					if(currentHotButton == selectedHotbutton_It) {
						currentHotButton->deselect();
						currentHotButton->setInvisible();
					}
					else
						if(currentHotButton->isVisible())
							currentHotButton->touchEnable();
				}
				// Re-enable touch on other display objects (currently just gauges)
				for (std::vector<Gauge>::iterator g = Gauges.begin(); g != Gauges.end(); g++){
					g->touchEnable();
				}
			}
		}
	}


	// Normal operation - menus not visible
	else {
		// Delete a gauge & corresponding PID when it is pressed
		for (std::vector<Gauge>::iterator currentGauge = Gauges.begin(); currentGauge != Gauges.end(); currentGauge++) {
			if(currentGauge->isPressed()) {
				int gX = currentGauge->getDOPosX();
				int gY = currentGauge->getDOPosY();
				string PIDLink = currentGauge->getPIDLink();
				cout << PIDLink << " Gauge released.. deleting.." << endl;
				Gauges.erase(currentGauge);
				for (std::vector<Button>::iterator currentHotButton = HotButtons.begin(); currentHotButton != HotButtons.end(); currentHotButton++) {
					if(currentHotButton->getDOPosX() == gX && currentHotButton->getDOPosY() == gY) {
						currentHotButton->setVisible();
						currentHotButton->touchEnable();
					}
				}
				// Erase corresponding PID if no other datalinks exist
				auto PID_It = find_if(PIDs.begin(), PIDs.end(), [&PIDLink](const PID& obj) {return obj.command.compare(PIDLink) == 0;});
				if(PID_It != PIDs.end()) {
					PID_It->dashboard_datalinks--;
					if(PID_It->dashboard_datalinks == 0 && PID_It->plot_datalinks == 0 && PID_It->log_datalinks == 0) {
						cout << "No remaining datalinks - deleting PID.." << endl;
						PIDs.erase(PID_It);	
					}	
				}
				break;
			}
		}
	}



}  // E N D DisplayObjectManager



// setupGraphics()
void setupGraphics(int* widthPtr, int* heightPtr) {	
	init(widthPtr,heightPtr);
	Start(*widthPtr, *heightPtr);		//Set up graphics, start picture
	Background(0,0,0);
}

