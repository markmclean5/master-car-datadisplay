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

// Prototypes
void setupGraphics(int*,int*);
void DisplayObjectManager(std::vector<Button>&, std::vector<Gauge>&, std::vector<PID>&, std::vector<Menu>&);
void modeManager (touch_t*);

// Mode Management enumeration definition
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

// Mode initialization states
bool dashboardModeInitialized = false;
bool plotModeInitialized = false;
bool logModeInitialized = false;
bool diagnosticModeInitialized = false;
bool informationModeInitialized = false;
bool settingsModeInitialized = false;
bool developmentModeInitialized = false;

ApplicationMode currentMode = noMode;
ApplicationMode previousMode = noMode;

//Dashboard mode vectors
vector<Button> DASHBOARD_HotButtons;
vector<Gauge> DASHBOARD_Gauges;
vector<PID> DASHBOARD_PIDs;
vector<Menu> DASHBOARD_Menus;

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

// main()
int main() {
	int width, height;					// display width & height
	setupGraphics(&width, &height);		// Initialize display

	if (!bcm2835_init())
		return 1;
	if (touchinit(width, height) != 0) {
		fprintf(stderr, "Unable to initialize the mouse\n");
		exit(1);
	}

	DASHBOARD_Menus.reserve(4);
	DASHBOARD_HotButtons.reserve(4);
	DASHBOARD_PIDs.reserve(4);
	DASHBOARD_Gauges.reserve(4);

	// Draw background
	Image(0, 0, 800, 480, "/home/pi/master-car-datadisplay/wallpaper.jpg");
	BackgroundImage = vgCreateImage(VG_sABGR_8888, 800, 480, VG_IMAGE_QUALITY_BETTER);
	vgGetPixels(BackgroundImage, 0, 0, 0, 0, 800, 480);
	

	Menu ModeMenu(width/2, height-30, width-20, 50, "ModeMenu");
	ModeMenuPtr = &ModeMenu;
	ModeMenu.touchEnable();
	Serial ELMSerial("/dev/ELM327", B38400);
	ELMSerial.serialWrite("ATZ");
	ELMSerial.setEndChar('>');
	ELMSerial.setReadTimeout(5000);


	// TODO: Move - Mode 1 temporary declarations
	TextView SerialViewer(width/3 - width/6, height/2, width/3, 360, "SerialViewer");
	int numPIDs = 0;
	std::vector<PID>::iterator p = DASHBOARD_PIDs.begin();			// Set up iterator for PID vector
	bool waitingOnData = false;
	bool menuVisible = false;

	ELMSerial.serialWrite("ATZ");
	string serialData = "";
	int gaugeCenterX = width/2;
	int gaugeCenterY = height/2-30;
	int gaugeRadius = height/2 - 80;



	//////////////////////////////////////
	// Main Execution Loop
	///////////////////////////////////////
	while(1) {
		loopTouch = threadTouch;											// Get touch for loop
		loopTime = bcm2835_st_read();										// Get time for loop
		serialData = ELMSerial.serialReadUntil();							// Get serial data for loop
		vgSetPixels(0, 0, BackgroundImage, 0, 0, 800, 480);					// Draw background image
		ModeMenu.update(&loopTouch);										// Update mode menu


		// TODO: decide if this needs touch and should update the menu - if so remove double update
		modeManager(&loopTouch);

		//////////////////////////////////////
		// Mode 1 - DASHBOARD
		//////////////////////////////////////


		// Dashboard mode run-time
		if(currentMode == dashboardMode) {

			if(DASHBOARD_PIDs.size() != numPIDs) {																				// Reset PID vector beginning iterator if vector size changes
				numPIDs = DASHBOARD_PIDs.size();
				p = DASHBOARD_PIDs.begin();
			}
			if(!waitingOnData && DASHBOARD_PIDs.size()>0) {																		// Request data if: no pending request
				ELMSerial.serialWrite((p)->getCommand());
				waitingOnData = true;
			}
			else if(waitingOnData && !serialData.empty() && DASHBOARD_PIDs.size()>0) {											// Update the current PID if data is present
				waitingOnData = false;
				(p)->update(serialData, loopTime);
				SerialViewer.addNewLine((p)->getCommand() + " - " + to_string((p)->getRawUpdateRate()) + " Hz");
				if(p<DASHBOARD_PIDs.end()) p++;
				if(p == DASHBOARD_PIDs.end()) p = DASHBOARD_PIDs.begin();
			}
			for (std::vector<Button>::iterator it = DASHBOARD_HotButtons.begin(); it != DASHBOARD_HotButtons.end(); it++) {		// Update all hot buttons
				(it)->updateTouch(&loopTouch);
				(it)->update();
			}
			for (std::vector<Gauge>::iterator it = DASHBOARD_Gauges.begin(); it != DASHBOARD_Gauges.end(); it++) {				// Update all gauges
				(it)->updateTouch(&loopTouch);
				string name = (it)->getPIDLink();
				if(it->isTouched()) cout << "Gauge " << name << " is touched!" << endl;
				auto CurrentGaugePID_It = find_if(DASHBOARD_PIDs.begin(), DASHBOARD_PIDs.end(), [&name](PID& obj) {return obj.getCommand().compare(name) == 0;});
				
				// TODO - Understand and fix
				if(CurrentGaugePID_It == DASHBOARD_PIDs.end()) {
					cout << "PID Not found" << endl;
					(it)->update(0,"RPM");
				} 
				else
					(it)->update((CurrentGaugePID_It)->getRawDatum(), (CurrentGaugePID_It)->getEngUnits());		
			}
			SerialViewer.update();
			// Update all menus
			for (std::vector<Menu>::iterator it = DASHBOARD_Menus.begin(); it != DASHBOARD_Menus.end(); it++) {
				(it)->update(&loopTouch);
			}
			// Run DisplayObjectManager (current page dashboard hotbuttons, display objects, and PIDs)
			DisplayObjectManager(DASHBOARD_HotButtons, DASHBOARD_Gauges, DASHBOARD_PIDs, DASHBOARD_Menus);				
		}

		End();				// Write picture to screen

		
		/*
		//////////////////////////////////////
		// Mode 4 - Communication test mode
		//////////////////////////////////////
		if(ModeMenu.isButtonSelected("m4")) {
			
			enum ConnectionStatus { disconnected, connecting, connected };

			enum ParmStatus { unknown, requested, known };

			
			string VIN 			= "";
			ParmStatus VINStat = unknown;
			string ELMVersion = "";
			ParmStatus ELMVersionStat = unknown;
			string Protocol = "";
			ParmStatus ProtocolStat = unknown;
			
			// ELM327
			ConnectionStatus ELMStatus = disconnected;
			uint64_t ELMConnectStartTime = 0;
			int ELMConnectTimeout = 1000; 					// Timeout, milliseconds
			string ELMResponseString = "";

			//ECU
			ConnectionStatus ECUStatus = disconnected;
			int ECUConnectTries = 0;
			string ECUResponseString = "";

			// Display items / setup
			TextView ConnectionStatusView (width/2, height/2-50, width/3-20, 360, "ConnectView");
			TextView ConnectStats(width-width/6, height/2 - 50, width/3-20, 360, "ConnectView");
			Button ConnectButton(width/6, 100, width/3 - 20, 70, "ConnectButton");
			ConnectButton.touchEnable();
			ConnectionStatusView.addNewLineIdentifier("ELMStatus", "ELM Status: ");
			ConnectionStatusView.setLineIdentifierText("ELMStatus", "Disconnected");
			ConnectionStatusView.addNewLineIdentifier("ELMVersion", "ELM Version: ");
			ConnectionStatusView.setLineIdentifierText("ELMVersion", "--------");
			ConnectionStatusView.addNewLineIdentifier("ECUStatus", "ECU Status: ");
			ConnectionStatusView.setLineIdentifierText("ECUStatus", "Disconnected");
			ConnectionStatusView.addNewLineIdentifier("ECUTries", "ECU Tries: ");
			ConnectionStatusView.setLineIdentifierText("ECUTries", "----");
			ConnectionStatusView.addNewLineIdentifier("Protocol", "Protocol: ");
			ConnectionStatusView.setLineIdentifierText("Protocol", "--------");
			ConnectionStatusView.addNewLineIdentifier("VIN", "VIN: ");
			ConnectionStatusView.setLineIdentifierText("VIN", "--------");			
			

			while(1) {
				// Loop guts
				loopTime = bcm2835_st_read();
				loopTouch = threadTouch;
				vgSetPixels(0, 0, BackgroundImage, 0, 0, 800, 480);
				ModeMenu.update(&loopTouch);

				// Page Graphics
				Fill(255,255,255,1);
				TextMid(width/2, height - 100, "Connection", SansTypeface, 20);
				TextMid(width-width/6, height - 100, "Vehicle Information", SansTypeface, 20);

				// Connect to the ELM327
				if(ConnectButton.isPressed() && ELMStatus == disconnected) {										
					ELMStatus = connecting;
					ConnectionStatusView.setLineIdentifierText("ELMStatus", "Connecting");
					ELMSerial.serialWrite("ATZ");
					ELMConnectStartTime = loopTime;
				}

				// Connecting to the ELM327
				if(ELMStatus == connecting && (loopTime < (ELMConnectStartTime + ELMConnectTimeout*1000))) {		
					ELMResponseString = ELMSerial.serialReadUntil();
					if(!ELMResponseString.empty()) {
						ELMStatus = connected;
						ConnectionStatusView.setLineIdentifierText("ELMStatus", "Connected");
						
						string resp = "";
						ELMSerial.serialWrite("ATE0"); // set elm327 output for NO ECHO
						while(resp.empty()) {
							resp = ELMSerial.serialReadUntil();
						}

						resp = "";
						ELMSerial.serialWrite("ATSP0");
						
						while(resp.empty()) {
							resp = ELMSerial.serialReadUntil();
						}
					}
				}

				// ELM327 connection timeout
				if(ELMStatus == connecting && (loopTime >= (ELMConnectStartTime + ELMConnectTimeout*1000))) {		
					ELMStatus = disconnected;
					ConnectionStatusView.setLineIdentifierText("ELMStatus", "Timeout");
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
						ConnectionStatusView.setLineIdentifierText("ELMVersion", ELMVersion );
					}

				}


				// Connect to the ECU after recieving ELM Version information
				if(ELMVersionStat == known && ECUStatus == disconnected) {
					ECUStatus = connecting;
					ConnectionStatusView.setLineIdentifierText("ECUStatus", "Connecting");
					ELMSerial.serialWrite("0101");

				}

				// Connecting to the ECU
				if(ECUStatus == connecting ) {
					ECUResponseString = ELMSerial.serialReadUntil();
					if(!ECUResponseString.empty()) {
						cout << ECUResponseString << endl;
						ECUConnectTries++;
						ConnectionStatusView.setLineIdentifierText("ECUTries", to_string(ECUConnectTries));
						if(ECUResponseString.find("41 01") != string::npos) {
							ECUStatus = connected;
							ConnectionStatusView.setLineIdentifierText("ECUStatus", "Connected");
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
						ConnectionStatusView.setLineIdentifierText("Protocol", Protocol );
					}

				}



				if(ProtocolStat == known && VINStat == unknown){
					// Request VIN: OBDII PID 0902 (mode 09; cmd 02)
					ELMSerial.serialWrite("0902");
					VINStat = requested;
				}



				
				if(VINStat == requested ){

					// Get raw VIN data: this is a string of hexidecimal characters
					VIN = ELMSerial.serialReadUntil(); // reads until prompt char received
					//VIN="090249 02 01 32 4D 45 46 49 02 02 4D 37 34 57 ";
					// this is a comment
					string decodedVIN = "";



					size_t found = 0;
					while(found!=string::npos){
						found = VIN.find("49 02", found); // returns pos of first occurence
						if(found != string::npos) {
							decodedVIN.append(VIN.substr(found+9, 12));
							found +=12;
						}
							
					}

					

					std::string::iterator end_pos = std::remove(decodedVIN.begin(), decodedVIN.end(), ' ');
					decodedVIN.erase(end_pos, decodedVIN.end());


					string finalVIN = "";
					for(int i=0;i<decodedVIN.length();i+=2){
						finalVIN += ( (char)strtoul(decodedVIN.substr(i, 2).c_str(), NULL, 16) );
						//cout << "stroutl " << (char)strtoul(decodedVIN.substr(i, 2).c_str(), NULL, 16) << endl;
					}
					
					if( !VIN.empty() ){
						ConnectionStatusView.setLineIdentifierText("VIN", finalVIN);
						VINStat = known;
					}

				}

				ConnectButton.updateTouch(&loopTouch);
				ConnectButton.update();

				ConnectionStatusView.update();
				ConnectStats.update();

				if(ModeMenu.isButtonPressed("m1")) {
					ModeMenu.selectButton("m1");
					break;
				}
				if(ModeMenu.isButtonPressed("m2")) {
					ModeMenu.selectButton("m2");
					break;
				}
				if(ModeMenu.isButtonPressed("m3")) {
					ModeMenu.selectButton("m3");
					break;
				}
				if(ModeMenu.isButtonPressed("m5")) {
					ModeMenu.selectButton("m5");
					break;
				}
				
				if(ModeMenu.isButtonPressed("m6")) {
					ModeMenu.selectButton("m6");
					break;
				}
				
				
				
				End();
			}

		}
		//////////////////////////////////////
		// Mode 5 - PID Support Check
		//////////////////////////////////////
		if(ModeMenu.isButtonSelected("m5")) {
			Menu PIDSupportMenu(width/6, height/2, width/3 - 20, 360, "PIDSupport");
			TextView PIDList(width/2, height/2, width/3-20, 360, "PIDList");

			Menu SupportedPIDMenu(width - width/6, height/2, width/3 - 20, 360, "SupportedPIDMenu");
			
			vector<PID> SupportPIDs;
			vector<PID> SupportedPIDs;

			
			
			while(1) {
				loopTime = bcm2835_st_read();
				loopTouch = threadTouch;
				vgSetPixels(0, 0, BackgroundImage, 0, 0, 800, 480);
				ModeMenu.update(&loopTouch);
				SupportedPIDMenu.update(&loopTouch);

				PIDSupportMenu.update(&loopTouch);
				string menuButton = PIDSupportMenu.getPressedButtonName();
				if(!menuButton.empty()) {
					PIDSupportMenu.selectButton(menuButton);
				}

				string pressedBtn = PIDSupportMenu.getPressedButtonName();
				if(!pressedBtn.empty()) {

				//if(!PIDSupportMenu.getSelectedButtonName().empty() && SupportPIDs.size() == 0) {
					cout << "requesting supported pids - " << pressedBtn << endl;
					SupportPIDs.emplace_back(pressedBtn);


					string simResponse = pressedBtn;
					simResponse[0] = '4';
					simResponse.append(" FF FF FF FF");

					SupportPIDs.back().update(simResponse, loopTime);

					for(int i = 0; i < SupportPIDs.back().getNumBits(); i++) {
						cout << i << " - Name "<< SupportPIDs.back().getBitPositionName(i) << " - State " << SupportPIDs.back().getBitPositionState(i) << endl;
						if(SupportPIDs.back().getBitPositionValue(i)) SupportedPIDMenu.addItem(SupportPIDs.back().getBitPositionName(i), SupportPIDs.back().getBitPositionName(i));
					}
				}
					
				PIDList.update();



				
				if(ModeMenu.isButtonPressed("m1")) {
					ModeMenu.selectButton("m1");
					break;
				}
				if(ModeMenu.isButtonPressed("m2")) {
					ModeMenu.selectButton("m2");
					break;
				}
				if(ModeMenu.isButtonPressed("m3")) {
					ModeMenu.selectButton("m3");
					break;
				}
				if(ModeMenu.isButtonPressed("m4")) {
					ModeMenu.selectButton("m4");
					break;
				}
				if(ModeMenu.isButtonPressed("m6")) {
					ModeMenu.selectButton("m6");
					break;
				}
				
				
				
				End();
			}

		}
		
		
		
		//////////////////////////////////////
		// Mode 6 - TERMINATE PROGRAM
		//////////////////////////////////////
		if(ModeMenu.isButtonSelected("m6")) {
			
			
			exit(EXIT_SUCCESS);
			
		}
		
		
		
		
		
		
		
		// Write screen buffer to screen
		End();

	*/
	}
}


// Mode management function
void modeManager (touch_t* menuTouch) {
	// Default mode upon initialization
	if(currentMode == noMode) {
		currentMode = dashboardMode;
		ModeMenuPtr->selectButton("m1");
	}
	ModeMenuPtr->update(menuTouch);
	string btnPressedString = ModeMenuPtr->getPressedButtonName();
	string prevModeString = ModeMenuPtr->getSelectedButtonName();

	// Change modes
	if(!btnPressedString.empty() && btnPressedString.compare(prevModeString) != 0) {
		//previousMode = currentMode;
		ModeMenuPtr->selectButton(btnPressedString);
		if(ModeMenuPtr->isButtonSelected("m1")) currentMode = dashboardMode;
		if(ModeMenuPtr->isButtonSelected("m2")) currentMode = plotMode;
		if(ModeMenuPtr->isButtonSelected("m3")) currentMode = logMode;
		if(ModeMenuPtr->isButtonSelected("m4")) currentMode = diagnosticMode;
		if(ModeMenuPtr->isButtonSelected("m5")) currentMode = informationMode;
		if(ModeMenuPtr->isButtonSelected("m6")) currentMode = interfaceMode;
		if(ModeMenuPtr->isButtonSelected("m7")) currentMode = settingsMode;
		if(ModeMenuPtr->isButtonSelected("m8")) currentMode = developmentMode;
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
				PIDs.emplace_back(HOTBUTTON_ParameterMenu_It->getSelectedButtonName());
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
				// Erase corresponding PID
				auto PID_It = find_if(PIDs.begin(), PIDs.end(), [&PIDLink](const PID& obj) {return obj.command.compare(PIDLink) == 0;});
				if(PID_It != PIDs.end()) {
					cout << "Also deleting PID.." << endl;
					PIDs.erase(PID_It);				
				}
				break;
			}
		}
	}



}



// setupGraphics()
void setupGraphics(int* widthPtr, int* heightPtr) {	
	init(widthPtr,heightPtr);
	Start(*widthPtr, *heightPtr);		//Set up graphics, start picture
	Background(0,0,0);
}

