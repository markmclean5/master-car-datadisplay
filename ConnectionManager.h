#ifndef ConnectionManager_H
#define ConnectionManager_H


#include "PID.h"
#include "Serial.h"
#include <vector>
#include <iostream>
#include <string>


using namespace std;

// Connection Status Enums
enum ConnectionStatus {
	disconnected,
	connecting,
	connected
};

enum ParmStatus {
	unknown,
	requested,
	known
};

void ConnectionManager(uint64_t  , Serial* , ConnectionStatus* , uint64_t* , ParmStatus*  ,string* , ConnectionStatus*  , int* , ParmStatus*  , string*  , ParmStatus*  , int*, std::vector<PID>& );




#endif