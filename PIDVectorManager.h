#ifndef PIDVECTORMANAGER_H
#define PIDVECTORMANAGER_H

#include "PID.h"
#include "Serial.h"
#include <vector>
#include "modeManager.h"
#include <iostream>
#include <bcm2835.h>

// PID Vector management enumeration definition
enum PIDVectorState {
						inactive,		// Active PID updates not required
						active,			// Active, not waiting on response from ELM
						busy,			// Waiting for response from ELM
						complete		// All PIDs in vector have been updated
};

void PIDVectorManager(PIDVectorState* , std::vector<PID>&, std::vector<PID>::iterator&, int*, Serial*, ApplicationMode*, uint64_t*, uint64_t*, uint64_t*);

#endif