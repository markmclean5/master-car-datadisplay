//PID Vector managment function: Handles update of PID vectors based on current application mode
#include "PIDVectorManager.h"

using namespace std;

void PIDVectorManager(PIDVectorState* PIDVectorCurrentState, std::vector<PID>& PIDs, std::vector<PID>::iterator& CurrentPID, int* numPIDs, Serial* ELMSerialPtr, ApplicationMode* currentMode, uint64_t* secondLastPIDVectorUpdateTime, uint64_t* lastPIDVectorUpdateTime, uint64_t* loopTime ) {
	if(*PIDVectorCurrentState == inactive && (PIDs.size() != 0)) {				// Initial vector activation
		*PIDVectorCurrentState = active;
		//cout << "Activating PID Vector" << endl;
		
		
	}

	
	if(PIDs.size() == 0 && *PIDVectorCurrentState == active){
		
		*PIDVectorCurrentState = inactive;
		//cout<< "Dactivating PID Vector" <<endl;
	}

	
	if(*PIDVectorCurrentState != inactive && *PIDVectorCurrentState != complete) {
		if(*numPIDs != PIDs.size()) {
			if(*PIDVectorCurrentState != busy) {								// Reset PID vector beginning iterator if vector modified
				*numPIDs = PIDs.size();		
				CurrentPID = PIDs.begin();
				//cout << "PID Vector modified - resetting iterator to start" << endl; 	
			}
			else {														// Discard requested data if vector modified
				string dump = ELMSerialPtr->serialReadUntil();
				if(!dump.empty()) *PIDVectorCurrentState = active;
			}
		}
		else if(*PIDVectorCurrentState == active) {
			//cout << "PID Vector active" << endl;
			if(		(*currentMode == dashboardMode && CurrentPID->dashboard_datalinks != 0) ||
					(*currentMode == plotMode && CurrentPID->plot_datalinks != 0) ||
					(*currentMode == logMode && CurrentPID->log_datalinks != 0)) {
				//cout << "Requirements met to update PID - setting vector busy" << endl;
				ELMSerialPtr->serialWrite((CurrentPID)->getCommand());		// Request PID data if necessary
				*PIDVectorCurrentState = busy;
			}
			else {
				//cout << "Requirements not net to update PID - moving to next" << endl;
				CurrentPID++;		// Move on to next PID
			}
		}

			
		}
		if(*PIDVectorCurrentState == busy) {								// Read PID response
			string serialResponse = ELMSerialPtr->serialReadUntil();
			if(!serialResponse.empty()) {
				cout << "Data recieved! updating PID with" << serialResponse << endl;
				(CurrentPID)->update(serialResponse, *loopTime);		
				if(CurrentPID < PIDs.end()) {
					CurrentPID++;										// On to the next one
					*PIDVectorCurrentState = active;
			}	
		}
		if(CurrentPID == PIDs.end()) {
			cout << "PID Vector update complete." << endl;
			*PIDVectorCurrentState = complete;							// All PIDs updated
			CurrentPID = PIDs.begin();

			// Capture vector update completion time
			*secondLastPIDVectorUpdateTime = *lastPIDVectorUpdateTime;
			*lastPIDVectorUpdateTime = bcm2835_st_read();
		} 
					
	}
}