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

// Managers
#include "modeManager.h"
#include "DisplayObjectManager.h"
#include "PIDVectorManager.h"
#include "ConnectionManager.h"

// Loop time
uint64_t loopTime;
touch_t threadTouch, loopTouch;
int quitState = 0;

VGImage BackgroundImage;

//Serial *ELMSerialPtr;														**** REMOVE??

// Prototypes
void setupGraphics(int*,int*);

// PID Vector update rate times
uint64_t lastPIDVectorUpdateTime = 0;
uint64_t secondLastPIDVectorUpdateTime = 0;

ApplicationMode currentMode = noMode;			// Current application mode
ApplicationMode previousMode = noMode;			// Previous application mode, for mode change detection

//Dashboard mode vectors and states
vector<Button> DASHBOARD_HotButtons;
vector<Gauge> DASHBOARD_Gauges;
vector<Menu> DASHBOARD_Menus;



// Everything to do with PIDs
vector<PID> PIDs;												// PID vector
int numPIDs = 0;													// PID counter for vector size change detection
std::vector<PID>::iterator CurrentPID = PIDs.begin();// Current PID in vector
PIDVectorState PIDVectorCurrentState = inactive;				// PID vector state for processing states
									


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
	ModeMenu.touchEnable();

	// Serial object for ELM327 interface
	Serial ELMSerial("/dev/ELM327", B38400);
	//ELMSerialPtr = &ELMSerial;																			**** REMOVE???
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
	Button NumSuportedPIDsButton(350+90, 18, 50, 35, "NumPIDsButton");
	
	// ELM connection and ECU connection status
	ConnectionStatus 	ELMStatus 					= disconnected;
	uint64_t ELMConnectStartTime 	= 0;


	string 						ELMVersion 					= "";
	ParmStatus 				ELMVersionStat 			= unknown;
	string 						Protocol 						= "";
	ParmStatus 				ProtocolStat 				= unknown;
	ConnectionStatus 	ECUStatus 					= disconnected;
	int 							ECUConnectTries 		= 0;
	string 						VIN 								= "";
	ParmStatus 				VINStat 						= unknown;
	
	uint64_t lastLoopTime = 0;
	
	
	// PID support 		
	vector<PID> SupportPIDs;			// PID support queries
	ParmStatus PIDSupportRequestStatus 	= unknown;
	int currentPIDSupportRequest 				= 100;
	bool PIDSupportRequestsComplete = false;


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


	 // PID TEST
	 
	 PID TestPID("TEST");
	 cout << "Successfully created Test PID" << endl;
	 
	 // END PID TEST
	 
	 
	 
/**********************************************************************************************************************************************************
																														M A I N    W H I L E    L O O P
**********************************************************************************************************************************************************/
	
	while(1) {
		
		// **** Global Loop  Parameters ****//
		
		loopTouch = threadTouch;											// Get touch for loop
		loopTime = bcm2835_st_read();										// Get time for loop

		vgSetPixels(0, 0, BackgroundImage, 0, 0, 800, 480);					// Draw background image
		
		
		
		ModeMenu.update(&loopTouch);										// Update mode menu
		
		
		
	/*

	// Not part of connection manager - generates menu / tables of supported PIDs
	// PIDSupportRequestsComplete is flag to run once
	if(PIDSupportRequestStatus == known && !PIDSupportRequestsComplete) {
		PIDSupportRequestsComplete 	= true;
		int numSupportPIDs = SupportPIDs.size();
		int PIDidx = 0;
		bool* PIDSupportStates = new bool[numSupportPIDs*31];
		string* PIDSupportNames = new string[numSupportPIDs*31];
		string* PIDSupportLabels = new string[numSupportPIDs*31];
		
		int numSupportedPIDs = 0;
		
		
		for(std::vector<PID>::iterator it = SupportPIDs.begin(); it != SupportPIDs.end(); it++)  {
			for(int p = 0; p<30; p++) {
				PIDSupportStates[PIDidx] = (it)->getBitPositionValue(p);
				PIDSupportNames[PIDidx] = (it)->getBitPositionName(p);
				PIDSupportLabels[PIDidx] = (it)->getBitPositionLabel(p);
					
				if(PIDSupportStates[PIDidx]) {
					numSupportedPIDs++;
				}
				PIDidx++;
			}
		}
		//NumSuportedPIDsButton
		NumSuportedPIDsButton.setValue(numSupportedPIDs);
	}
	
	*/
	
	
	
	
	
	
		// Framerate button - need to enable / disable
		float refreshRate = 1000000/(loopTime - lastLoopTime);
		lastLoopTime = loopTime;
		FramerateButton.setValue(refreshRate);
		FramerateButton.update();
		
		/******************************************************************************************
		Manager Function Calls
		******************************************************************************************/
		// Mode Management
		modeManager(&ModeMenu, &loopTouch, &previousMode, &currentMode);
		
		/*
		// Connection Management (ELM, ECU,Protocol detection, PID support request)
		ConnectionManager( loopTime ,  &ELMSerial,  &ELMStatus, &ELMConnectStartTime, &ELMVersionStat , &ELMVersion, &ECUStatus , &ECUConnectTries, &ProtocolStat , &Protocol , &PIDSupportRequestStatus , &currentPIDSupportRequest, SupportPIDs);
		
		
		// PID Vector Management
		if(PIDSupportRequestStatus == known) {
			PIDVectorManager(&PIDVectorCurrentState, PIDs, CurrentPID, &numPIDs, &ELMSerial, &currentMode, &secondLastPIDVectorUpdateTime, &lastPIDVectorUpdateTime, &loopTime);
		}
		
		
		if (PIDVectorCurrentState == complete) PIDVectorCurrentState = active;	// Loop PID vector upon update completion


		/******************************************************************************************
		Mode 1 - DASHBOARD
		******************************************************************************************/
		
		/*
		
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
			
			DisplayObjectManager(DASHBOARD_HotButtons, DASHBOARD_Gauges, PIDs, SupportPIDs, &PIDSupportRequestStatus, DASHBOARD_Menus);								// Run DisplayObjectManager (current page dashboard hotbuttons, display objects, and PIDs)		
		} // End Mode 1

		*/
		
		/******************************************************************************************
		Mode 2 - PLOT
		******************************************************************************************/
		// Plot mode run-time
		if(currentMode == plotMode) {
			
		} // End Mode 2

		
		

		
		/******************************************************************************************
		Mode 3 - LOG
		******************************************************************************************/
		
		/*
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
		
		/*
		if(currentMode == developmentMode) {
		
		// Connection, protocol, and PID support statusing display
		if(ELMStatus == connecting) {
			ELMConnectionStatusButton.setText("ELM Connecting");
		}
		if(ELMStatus == connected) {
			ELMConnectionStatusButton.setText("ELM Connected");
		}
		if(ELMVersionStat == known) {
			ELMConnectionStatusButton.setText(ELMVersion);
		}
		if(ECUStatus == connecting) {
			string txt = "ECU Connecting - ";
			txt.append(std::to_string(ECUConnectTries));
			ECUConnectionStatusButton.setText(txt);
		}
		if(ECUStatus == connected) {
			string txt = "ECU Connected";
		}
		if(ProtocolStat == known) {
			ECUConnectionStatusButton.setText(Protocol);
		}

		
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
		
		//NumSuportedPIDsButton
		NumSuportedPIDsButton.update();
		
		} // End Mode 8
		
		
		*/
		

		End();				// Write picture to screen
		
	}  // E N D    M A I N    W H I L E
}  // E N D    M A I N   M E T H O D

/**********************************************************************************************************************************************************
																								COMPONENT FUNCTIONS {TO BECOME SEPERATE CPP FILES}
**********************************************************************************************************************************************************/

// setupGraphics()
void setupGraphics(int* widthPtr, int* heightPtr) {	
	init(widthPtr,heightPtr);
	Start(*widthPtr, *heightPtr);		//Set up graphics, start picture
	Background(0,0,0);
}

