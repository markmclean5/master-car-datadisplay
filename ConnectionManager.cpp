
#include "ConnectionManager.h"



void ConnectionManager(uint64_t loopTime , Serial* ELMSerial, ConnectionStatus* ELMStatus, uint64_t* ELMConnectStartTime, ParmStatus* ELMVersionStat ,string* ELMVersion, ConnectionStatus* ECUStatus , int* ECUConnectTries, ParmStatus* ProtocolStat , string* Protocol , ParmStatus* PIDSupportRequestStatus , int* currentPIDSupportRequest, std::vector<PID>& SupportPIDs){


//Initialize variables
	int 		ELMConnectTimeout 	= 1000; 					// Timeout, milliseconds



// Autoconnection and connection statusing logic	
	if(*ELMStatus == disconnected) {
		*ELMStatus = connecting;
		ELMSerial->serialWrite("ATZ"); //ELM reset
		*ELMConnectStartTime = loopTime;		
	}
	if(*ELMStatus == connecting && (loopTime < (*ELMConnectStartTime + ELMConnectTimeout*1000))) {
		string ELMResponseString = ELMSerial->serialReadUntil();
			if(!ELMResponseString.empty()) {
				*ELMStatus = connected;
				//ELMConnectionStatusButton.setText("ELM Connected");
					string resp = "";
					ELMSerial->serialWrite("ATE0"); // set elm327 output for NO ECHO
					while(resp.empty()) {
						resp = ELMSerial->serialReadUntil();
					}
					resp = "";
					ELMSerial->serialWrite("ATSP0");	// set ELM327 to automatically detect protocol
					while(resp.empty()) {
						resp = ELMSerial->serialReadUntil();
					}
			}
	}
	// ELM327 connection timeout
	if(*ELMStatus == connecting && (loopTime >= (*ELMConnectStartTime + ELMConnectTimeout*1000))) {		
		*ELMStatus = disconnected;
		//ELMConnectionStatusButton.setText("ELM Timeout");
	}
	// Reqest ELM327 Version information
	if(*ELMStatus == connected && *ELMVersionStat == unknown) {			
		ELMSerial->serialWrite("ATI");
		*ELMVersionStat = requested;
	}

	// Requesting ELM327 Version information
	if(*ELMVersionStat == requested) {
		*ELMVersion = ELMSerial->serialReadUntil();
		if(!ELMVersion->empty()){
			*ELMVersionStat = known; 
			//ELMConnectionStatusButton.setText(*ELMVersion);
		}
	}
	
	// Connect to the ECU after recieving ELM Version information
	if(*ELMVersionStat == known && *ECUStatus == disconnected) {
		*ECUStatus = connecting;
		ELMSerial->serialWrite("0101");
	}
	
	// Connecting to the ECU
	if(*ECUStatus == connecting ) {
		string ECUResponseString = ELMSerial->serialReadUntil();
		if(!ECUResponseString.empty()) {
			cout << ECUResponseString << endl;
			*ECUConnectTries++;
			
			if(ECUResponseString.find("41 01") != string::npos) {
				*ECUStatus = connected;
			}
			else {
				*ECUStatus = disconnected;
				ECUResponseString = "";
			}
		}

	}

	// Request current connection protocol information after connecting to the ECU
	if(*ECUStatus == connected && *ProtocolStat == unknown){
		ELMSerial->serialWrite("ATDP");
		*ProtocolStat = requested;
	}

	// Requesting protocol information
	if(*ProtocolStat == requested) {
		*Protocol = ELMSerial->serialReadUntil();
		if(!Protocol->empty()){
			*ProtocolStat = known; 
			//ECUConnectionStatusButton.setText(*Protocol);
		}

	}
	
	// Requestiing PID support from ECU (assuming protocol known)
	if(*ProtocolStat == known && *PIDSupportRequestStatus == unknown) {
		string requestPIDString = "0";
		requestPIDString.append(std::to_string(*currentPIDSupportRequest) );	//"0100" first time around
		SupportPIDs.emplace_back(requestPIDString);
		ELMSerial->serialWrite(SupportPIDs.back().getCommand());
		*PIDSupportRequestStatus = requested;
	}
	
	// Waiting on some PID support request response
	if(*PIDSupportRequestStatus == requested) {
		string resp = ELMSerial->serialReadUntil();
		if(!resp.empty()) {
			SupportPIDs.back().update(resp, loopTime);
			
			cout << " we think 0120 support state is here: " << SupportPIDs.back().getBitPositionName(31) << endl;
		
			// Move on to next PID support request
			if(SupportPIDs.back().getBitPositionValue(31) && *currentPIDSupportRequest < 160) {
				*PIDSupportRequestStatus = unknown;
				*currentPIDSupportRequest += 20;
			}
			// Or stop here (all PID support queries complete)
			else
				*PIDSupportRequestStatus = known;
		}
	}
	
} // END CONNECTION MANAGER