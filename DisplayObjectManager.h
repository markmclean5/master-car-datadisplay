
#ifndef DisplayObjectManager_H
#define DisplayObjectManager_H

# include <stdlib.h>		//
# include <vector>
# include "Button.h"
# include "Gauge.h"
# include "PID.h"
# include "Menu.h"
#include <iostream>
#include <algorithm>		// find_if

using namespace std;

void DisplayObjectManager(std::vector<Button>&, std::vector<Gauge>&, std::vector<PID>&, std::vector<Menu>&);



#endif