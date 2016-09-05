/********************************/
/*	PID Class					*/
/********************************/
#include <sys/types.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>			// Math (float remainder operation)
#include <math.h>
#include "PID.h"
#include <bcm2835.h>
#include <algorithm>    // std::find
#include <string>
#include <locale.h>
#include <config4cpp/Configuration.h>
#include "parsingUtilities.h"

using namespace std;
using namespace config4cpp;

PID::PID(string PIDid) {
	cout << "PID created: " << PIDid << endl; 
	id = PIDid;
	debug = false;
	configure(PIDid);
	currentTime = 0;
	lastTime = 0;
	updateRate = 0;
	lastUpdateTime = 0;

	dashboard_datalinks = 0;
	plot_datalinks = 0;
	log_datalinks = 0;

}

/* PID configure method */
void PID::configure(string ident) {
	setlocale(LC_ALL, "");
	Configuration * cfg = Configuration::create();
	try {
		cfg->parse("/home/pi/master-car-datadisplay/PIDConf");
		string PIDName = ident;
		
		
		fullName = parseString(cfg, PIDName, "fullName");
		shortName = parseString(cfg, PIDName, "shortName");
		
		
		//type = parseString(cfg, PIDName, "type");							// -move to each element
		command = parseString(cfg, PIDName, "command");
		numDataBytes = parseInt(cfg, PIDName, "numDataBytes");


		// New stuff
		numElements = parseInt(cfg, PIDName, "numElements");
		types = new string[numElements];
		// Get number of each type of PID data elements
		numValueElements = 0;
		numBitEncodedElements = 0;
		numEnumeratedElements = 0;
		// Count the instances of each element type
		string elementScope = "element";
		
		for(int currentElement = 1;currentElement<=numElements;currentElement++) {
			string currentElementScope = elementScope + to_string(currentElement);
			string elementType = parseString(cfg, PIDName, currentElementScope, ".type");

			if(elementType.compare("value") == 0) {
				numValueElements++;
			}
			else if(elementType.compare("bit-encoded") == 0) {
				numBitEncodedElements++;
			}
			else if(elementType.compare("enumerated") == 0) {
				numEnumeratedElements++;
			}
		}

		// Dynamic creation of value element attributes
		if(numValueElements != 0) {
			valueStartBytes = new char[numValueElements];
			numValueBytes = new int[numValueElements];
			supportedMinVals = new float[numValueElements];
			supportedMaxVals = new float[numValueElements];
			numRanges = new int[numValueElements];
			TotalGains = new float[numValueElements];
			TotalOffsets = new float[numValueElements];

			// To be further allocated later on when number of ranges for each element are known
			byteGains = new float*[numValueElements];
			byteOffsets = new float*[numValueElements];
			rangeScalings = new float*[numValueElements];
			rangeStarts = new float*[numValueElements];
			rangeStops = new float*[numValueElements];

		}

		// Dynamic creation of bit encoded element attributes
		if(numBitEncodedElements != 0) {
			bitValues = new uint32_t[numBitEncodedElements];
			numBitEncodedBits = new int[numBitEncodedElements];
			bitEncodedStartBits = new int[numBitEncodedElements];
			
			// To be further allocated later on when number of bits for each element is known
			bitNames = new string*[numBitEncodedElements];
			bit0States = new string*[numBitEncodedElements];
			bit1States = new string*[numBitEncodedElements];
			bitLabels = new string*[numBitEncodedElements];
		}

		// Dynamic creation of enumerated element attributes
		if(numEnumeratedElements != 0){
			numEnums = new int[numEnumeratedElements];
			enumStartBits = new int[numEnumeratedElements];
			numEnumBits = new int[numEnumeratedElements];
			numVals = new int[numEnumeratedElements];

			// To be further allocated later on when number of bits and enumeration values are known
			enumVals = new int*[numEnumeratedElements];
			enumStates = new string*[numEnumeratedElements];
		}


		// Loop through all elements of the PID and do all the configuration work {
			
			// Indices of each element type
			int valueElementIdx = 0;
			int bitEncodedElementIdx = 0;
			int enumElementIdx = 0;
			
			for (int currentElement = 1; currentElement <= numElements; currentElement++) {
				string currentElementScope = elementScope + to_string(currentElement);
				string elementType = parseString(cfg, PIDName, currentElementScope, ".type");					// Maybe create a PID wide array of types?? (double parsing this)
				if(elementType.compare("value") == 0) {									// Value type PID element configuration
				
					// Value max and min (before range scalings)
					supportedMinVals[valueElementIdx] = parseFloat(cfg, PIDName, currentElementScope, ".min_val");
					supportedMaxVals[valueElementIdx] = parseFloat(cfg, PIDName, currentElementScope, ".max_val");
					
					// Number of bytes
					numValueBytes[valueElementIdx] = parseInt(cfg, PIDName, currentElementScope, ".num_bytes");

					// Allocate space for gains and offsets for each byte
					byteGains[valueElementIdx] = new float[numValueBytes[valueElementIdx]];
					byteOffsets[valueElementIdx] = new float[numValueBytes[valueElementIdx]];
					
					// Value start byte ('A', 'B'....)
					valueStartBytes[valueElementIdx] = parseString(cfg, PIDName, currentElementScope, ".start_byte")[0];		// Trying to get first character of string
					
					// Parse all byte gains and offsets
					for(int parseByte = 0; parseByte < numValueBytes[valueElementIdx]; parseByte++) {
						char byteScope = valueStartBytes[valueElementIdx]+=parseByte;
						byteGains[valueElementIdx][parseByte] = parseFloat(cfg, PIDName, currentElementScope, "."+ std::to_string(byteScope) + "_gain");
						byteOffsets[valueElementIdx][parseByte] = parseFloat(cfg, PIDName, currentElementScope, "."+ std::to_string(byteScope) + "_offset");
					}
					// Parse total gain and offset (applied to final value)
					TotalGains[valueElementIdx] = parseFloat(cfg, PIDName, ".total_gain");
					TotalOffsets[valueElementIdx] = parseFloat(cfg, PIDName, currentElementScope, ".total_offset");
		
					// Parse number of ranges
					numRanges[valueElementIdx]= parseInt(cfg, PIDName, currentElementScope, ".num_ranges");
						
					// Allocate all double-pointers for current element range specific attributes now that number of ranges is known
					rangeScalings[valueElementIdx] = new float[numRanges[valueElementIdx]];
					rangeStarts[valueElementIdx] = new float[numRanges[valueElementIdx]];
					rangeStops[valueElementIdx] = new float[numRanges[valueElementIdx]];
					EngUnits[valueElementIdx] = new string[numRanges[valueElementIdx]];
						
					// Parse attributes of each range
					for(int currentParseRange = 1; currentParseRange<=numRanges[valueElementIdx]; currentParseRange++) {
						string currentRangeScope = "range" + to_string(currentParseRange);
						rangeScalings[valueElementIdx][currentParseRange-1] = parseFloat(cfg, PIDName +"."+ currentElementScope, currentRangeScope, ".scaling");
						rangeStarts[valueElementIdx][currentParseRange-1] = parseFloat(cfg, PIDName + "." + currentElementScope, currentRangeScope, ".rangeStart");
						rangeStops[valueElementIdx][currentParseRange-1] = parseFloat(cfg, PIDName + "." + currentElementScope, currentRangeScope, ".rangeStop");
						EngUnits[valueElementIdx][currentParseRange-1] = parseString(cfg, PIDName + "." + currentElementScope, currentRangeScope, ".engUnits");
					}
					valueElementIdx++;
				} // End of value element parsing loop
				
				
			} // End of element-parsing loooop
			
		
		
		/// ----------------------------------------------------------------------------------------------


		/*
		else if(type.compare("bit-encoded") == 0) {						// Bit-encoded PID configuration
			char byt = 'A';
			int messageBit = 0;
			if(numDataBytes > 0) {
				for(int bit = 7; bit >=0; bit--) {
					cout << "Message Bit " << messageBit << " - " << byt+to_string(bit) << endl;
					cout << "Parsing " << byt+to_string(bit) << endl;
					bitNames[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_name");
					cout << "  Name = " << bitNames[messageBit] << endl;
					bitLabels[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_label");
					cout << "  Label = " << bitLabels[messageBit] << endl;
					bit0States[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_0state");
					cout << "  0 state = " << bit0States[messageBit] << endl;
					bit1States[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_1state");
					cout << "  1 state = " << bit1States[messageBit] << endl;
					messageBit++;
				}
			}
			if(numDataBytes > 1){
				byt = 'B';
				for(int bit = 7; bit >= 0; bit--) {
					cout << "Message Bit " << messageBit << " - " << byt+to_string(bit) << endl;
					cout << "Parsing " << byt+to_string(bit) << endl;
					bitNames[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_name");
					cout << "  Name = " << bitNames[messageBit] << endl;
					bitLabels[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_label");
					cout << "  Label = " << bitLabels[messageBit] << endl;
					bit0States[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_0state");
					cout << "  0 state = " << bit0States[messageBit] << endl;
					bit1States[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_1state");
					cout << "  1 state = " << bit0States[messageBit] << endl;
					messageBit++;
				}
			}
			if(numDataBytes > 2){
				byt = 'C';
				for(int bit = 7; bit >= 0; bit--) {
					cout << "Message Bit " << messageBit << " - " << byt+to_string(bit) << endl;
					cout << "Parsing " << byt+to_string(bit) << endl;
					bitNames[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_name");
					cout << "  Name = " << bitNames[messageBit] << endl;
					bitLabels[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_label");
					cout << "  Label = " << bitLabels[messageBit] << endl;
					bit0States[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_0state");
					cout << "  0 state = " << bit0States[messageBit] << endl;
					bit1States[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_1state");
					cout << "  1 state = " << bit0States[messageBit] << endl;
					messageBit++;
				}
			}
			if(numDataBytes > 3){
				byt = 'D';
				for(int bit = 7; bit >= 0; bit--) {
					cout << "Message Bit " << messageBit << " - " << byt+to_string(bit) << endl;
					cout << "Parsing " << byt+to_string(bit) << endl;
					bitNames[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_name");
					cout << "  Name = " << bitNames[messageBit] << endl;
					bitLabels[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_label");
					cout << "  Label = " << bitLabels[messageBit] << endl;
					bit0States[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_0state");
					cout << "  0 state = " << bit0States[messageBit] << endl;
					bit1States[messageBit] = parseString(cfg, PIDName, byt+to_string(bit)+"_1state");
					cout << "  1 state = " << bit0States[messageBit] << endl;
					messageBit++;
				}
			}

		}
		*/
		
		
	}catch(const ConfigurationException & ex) {
		cout << ex.c_str() << endl;
	}
	cfg->destroy();

	cout << "Reached end of configure!" << endl;
}

/*

string PID::getCommand(void) {
	return command;
}


// Getters: DataStream outputs
string PID::getEngUnits(void) {
	return EngUnits[currentRange-1];
}

float PID::getRawDatum(void) {
	// re-write
	float apples = 0;
	return apples;
}

float PID::getRawUpdateRate(void)
{
	return updateRate;
}

void PID::update (string serialData, uint64_t updateTime) {
	cout << "PID update called for: " << getCommand() << endl;
	cout << "  With " << serialData << endl;
	currentTime = updateTime;
	uint64_t timeDelta = currentTime - lastTime;
	lastTime = currentTime;
	updateRate = 1000000./timeDelta;
	string responseID = id;
	responseID[0] = '4';
	if(!serialData.empty()){
		// Remove spaces from serial data string (this might not work)
		std::string::iterator end_pos = std::remove(serialData.begin(), serialData.end(), ' ');
		serialData.erase(end_pos, serialData.end());
		size_t found = serialData.find(responseID);
		if(found != std::string::npos) {
			float value = 0;
			string dataByteString = serialData.substr(found+4, numDataBytes*2);
			//cout << "Data Byte String - " << dataByteString << endl;
			//cout << "num data bytes: " << numDataBytes << endl;
			

			if(type.compare("value") == 0) {
				for(int i=0;i<numDataBytes;i++) {
					//cout << "byte gain " << byteGain[i] << endl;
					//cout << "byte offset " << byteOffset[i] << endl;
					//cout << " substring " <<  dataByteString.substr(2*i, 2).c_str() << endl;
					//cout << "stroutl " << strtoul(dataByteString.substr(2*i, 2).c_str(), NULL, 16) << endl;
					value += (byteGain[i]*strtoul(dataByteString.substr(2*i, 2).c_str(), NULL, 16) + byteOffset[i]);
					//cout << "value before total " << value << endl;
				}
				//cout << "TotalGain " << TotalGain << endl;
				//cout << "TotalOffset " << TotalOffset << endl;
				value = (value*TotalGain) + TotalOffset;
				cout << " Value calculated to be: " << value <<endl;
				// Now find out which range the  value belongs to
				int range = 0;
				bool rangeFound = false;
				while(!rangeFound) {
					if(range==numRanges) break;
					if(debug) {
						//cout << "Checking if data fits within range # " << range+1 << endl;
						//cout << "Range start: " << rangeStart[range] << endl;
						//cout << "Range stop: " << rangeStop[range] << endl;
					}	
					if(((value>=rangeStart[range]) && (value<rangeStop[range])) ||
						((value>=rangeStop[range]) && (value<rangeStart[range]))) {
						if(debug) cout << "Data found in range # " << range+1 << endl;
						lastRange = currentRange;
						currentRange = range+1;
						rangeFound = true;
					}
					range++;
				}
				if(rangeFound==true) {
					value = value * rangeScaling[currentRange-1];	// Apply scaling of current range
					
					// looks like value is the desired data
				}
				else {
					cout << "Error: Data not inside any range. " << endl;
					currentRange=1;
				}
			}

			// Process a bit-encoded data string: 
			else if (type.compare("bit-encoded") == 0) {
				cout << "Processing bit-encoded PID" << endl;
				cout << "  substring: " << dataByteString.substr(0, 2*numDataBytes) << endl;
				bitValue =  strtoul(dataByteString.substr(0, 2*numDataBytes).c_str(), NULL, 16);
				cout << "  value: " << bitValue << endl;
			}
		}

	}

	else {
		currentRange = 1;
		cout << "PID updated without data" << endl;
	}

	cout << "At end of PID update: current range: " << currentRange << endl;
}

// Bit-encoded functions

bool PID::getBitNameValue(string name) {
	bool val = false;
	for(int i = 0; i < 8*numDataBytes; i++) {
		if(name.compare(bitNames[i]) == 0) {
			val = getBitPositionValue(i);
			break;
		}
	}
	return val;
}
string PID::getBitNameState(string name) {
	string state = "";
	for(int i = 0; i < 8*numDataBytes; i++) {
		if(name.compare(bitNames[i]) == 0) {
			state = getBitPositionState(i);
			break;
		}
	}
	return state;
}
bool PID::getBitPositionValue(int pos) {
	return ((bitValue >> (31-pos) ) &  1);
}
string PID::getBitPositionState(int pos) {
	if(getBitPositionValue(pos))
		return bit1States[pos];
	else
		return bit0States[pos];

}
string PID::getBitPositionName(int pos) {
	return bitNames[pos];
}

string PID::getBitPositionLabel(int pos) {
	return bitLabels[pos];
}

 
int PID::getNumBits(void) {
	return numDataBytes*8;
}

*/





