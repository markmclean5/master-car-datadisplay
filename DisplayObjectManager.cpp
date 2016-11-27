# include "DisplayObjectManager.h"

using namespace std;

void DisplayObjectManager(std::vector<Button>& HotButtons, std::vector<Gauge>& Gauges, std::vector<PID>& PIDs, std::vector<PID>& SupportPIDs, ParmStatus* PIDSupportRequestStatus, std::vector<Menu>& Menus, std::vector<TextView>& TextViews){

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
			
			Menus.emplace_back(width/2, 45, (width)- 20, 60, "HOTBUTTON_DisplayObjectMenu");
			Menus.emplace_back(width/12, (height/2)+12, (width/6) - 10, height-125, "HOTBUTTON_GroupMenu");
			string defaultGroup = "standardPIDMenu";
			Menus.back().selectButton(defaultGroup);
			Menus.emplace_back(width/2 -width/6, (height/2)+12, (width/3)- 10, height-125, defaultGroup);
			
			// Not part of connection manager - generates menu / tables of supported PIDs
			// PIDSupportRequestsComplete is flag to run once
			if(*PIDSupportRequestStatus == known) {
				int numSupportPIDs = SupportPIDs.size();
				int PIDidx = 0;
				bool* PIDSupportStates = new bool[numSupportPIDs*31];
				string* PIDSupportNames = new string[numSupportPIDs*31];
				string* PIDSupportLabels = new string[numSupportPIDs*31];
				for(std::vector<PID>::iterator it = SupportPIDs.begin(); it != SupportPIDs.end(); it++)  {
					for(int p = 0; p<30; p++) {
						PIDSupportStates[PIDidx] = (it)->getBitValue("supportedPIDs", p);
						PIDSupportNames[PIDidx] = (it)->getBitName("supportedPIDs", p);
						PIDSupportLabels[PIDidx] = (it)->getBitLabel("supportedPIDs", p);
						
						if(PIDSupportStates[PIDidx]) {
							Menus.back().addItem(PIDSupportNames[PIDidx], PIDSupportLabels[PIDidx]);
						}
						PIDidx++;
					}
				}
			}
		}

		// If hotbutton is selected - set selected button iterator
		if(currentHotButton->isSelected()) selectedHotbutton_It = currentHotButton;
	}

	// If menus are present
	if(Menus.size() != 0) {

		string name = "HOTBUTTON_GroupMenu";
		auto HOTBUTTON_GroupMenu_It = find_if(Menus.begin(), Menus.end(), [&name](const Menu& obj) {return obj.menuIdentifier.compare(name) == 0;});
		name = "HOTBUTTON_DisplayObjectMenu";
		auto HOTBUTTON_DisplayObjectMenu_It = find_if(Menus.begin(), Menus.end(), [&name](const Menu& obj) {return obj.menuIdentifier.compare(name) == 0;});
		string type = "ParameterMenu";
		auto HOTBUTTON_ParameterMenu_It = find_if(Menus.begin(), Menus.end(), [&type](const Menu& obj) {return obj.menuType.compare(type) == 0;});


		// Go through all menus and select pressed buttons
		for (std::vector<Menu>::iterator currentMenu = Menus.begin(); currentMenu != Menus.end(); currentMenu++) {
			string pressedButtonName = currentMenu->getPressedButtonName();
			if(!pressedButtonName.empty()) currentMenu->selectButton(pressedButtonName);
		}
				
		if(HOTBUTTON_GroupMenu_It->getSelectedButtonName().compare(HOTBUTTON_ParameterMenu_It->menuIdentifier) != 0) {
			Menus.erase(HOTBUTTON_ParameterMenu_It);
			Menus.emplace_back(width/2, (height/2)-30, (width/3)- 10, height-70, HOTBUTTON_GroupMenu_It->getSelectedButtonName());
			type = HOTBUTTON_GroupMenu_It->getSelectedButtonName();
			HOTBUTTON_ParameterMenu_It = find_if(Menus.begin(), Menus.end(), [&type](const Menu& obj) {return obj.menuType.compare(type) == 0;});
		}
		
		
		// If PID Group menu selected (std/ext) and parameter (specific PID selected), display textview with PID data
		if(!HOTBUTTON_GroupMenu_It->getSelectedButtonName().empty()
			&& !HOTBUTTON_ParameterMenu_It->getSelectedButtonName().empty()) {
				
			
			if(TextViews.size()==0) {
				// Check if PID already exists within the PID vector
				std::vector<PID>::iterator p = PIDs.begin();
				for (;p != PIDs.end(); p++){
					if(p->command.compare(HOTBUTTON_ParameterMenu_It->getSelectedButtonName())==0) {
						cout << "Temprary display PID exists in vector" << endl;
						p->temp_datalinks++;
						// Investigate need for datalinks
						break;
					}
				}
				// If the PID does not exist, add it
				if(p == PIDs.end()) {
					PIDs.emplace_back(HOTBUTTON_ParameterMenu_It->getSelectedButtonName());
					PIDs.back().temp_datalinks++;
					cout << "Added temporary display PID to vector" << endl;
				}
			
				int textViewWidth = width/3 - 10;
				int textViewHeight = (height-70) / PIDs.back().numElements;
				
				for(int i = 0; i < PIDs.back().numElements; i++) {
					TextViews.emplace_back(width - width/3 - 5, height - (i*textViewHeight) - textViewHeight/2, textViewWidth, textViewHeight, "PIDElementView");
					TextViews.back().addNewLineIdentifier("name", "Name: ");
					TextViews.back().setLineIdentifierText("name", PIDs.back().elementNames[i]);
					TextViews.back().addNewLineIdentifier("description", "Description: ");
					TextViews.back().setLineIdentifierText("description", PIDs.back().elementDescriptions[i]);
				}
			}
			
			else{
				std::vector<PID>::iterator p = PIDs.begin();
				for(std::vector<PID>::iterator p = PIDs.begin();p!=PIDs.end();p++){
					if(p->temp_datalinks != 0) {
						break;
					}
				}
				if(p!= PIDs.end()){
					if(HOTBUTTON_ParameterMenu_It->getSelectedButtonName().compare(p->id) != 0) {
						PIDs.erase(p);
						TextViews.clear();
					}
				}
				else {
					// Something bad happened - could not find PID with temp datalinks
				}
				
			}
			
			
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
					if(p->command.compare(HOTBUTTON_ParameterMenu_It->getSelectedButtonName())==0) {
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