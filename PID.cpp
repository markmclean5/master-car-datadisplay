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
		
		
		fullName = parseString(cfg, PIDName, "full_name");
		shortName = parseString(cfg, PIDName, "short_name");
		
		
		//type = parseString(cfg, PIDName, "type");							// -move to each element
		command = parseString(cfg, PIDName, "command");
		numDataBytes = parseInt(cfg, PIDName, "num_data_bytes");


		numElements = parseInt(cfg, PIDName, "num_elements");
		elementTypes = new string[numElements];
		elementNames = new string[numElements];
		elementDescriptions = new string[numElements];
		
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
				cout << "Value type element detected!" << endl;
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
			values = new float[numValueElements];
			
			// To be further allocated later on when number of ranges for each element are known
			byteGains = new float*[numValueElements];
			byteOffsets = new float*[numValueElements];
			rangeScalings = new float*[numValueElements];
			rangeStarts = new float*[numValueElements];
			rangeStops = new float*[numValueElements];
			EngUnits = new string*[numValueElements];
		}

		// Dynamic creation of bit encoded element attributes
		if(numBitEncodedElements != 0) {
			payload = 0;
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
				string elementType = parseString(cfg, PIDName, currentElementScope, ".type");
				elementTypes[currentElement-1] = elementType;
				elementNames[currentElement-1] = parseString(cfg, PIDName, currentElementScope, ".name");
				elementDescriptions[currentElement-1] = parseString(cfg, PIDName, currentElementScope, ".description");
				
				// Value type PID element configuration
				if(elementType.compare("value") == 0) {									
					cout << "Value type element being parsed!" << endl;
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
					cout << "Value type element starts at byte " << valueStartBytes[valueElementIdx] << endl;
					// Parse all byte gains and offsets
					for(int parseByte = 0; parseByte < numValueBytes[valueElementIdx]; parseByte++) {
						string attr = ".";
						char byteScope = valueStartBytes[valueElementIdx]+parseByte;
						attr+=byteScope;
						byteGains[valueElementIdx][parseByte] = parseFloat(cfg, PIDName, currentElementScope, attr + "_gain");
						byteOffsets[valueElementIdx][parseByte] = parseFloat(cfg, PIDName, currentElementScope, attr + "_offset");
					}
					// Parse total gain and offset (applied to final value)
					TotalGains[valueElementIdx] = parseFloat(cfg, PIDName, currentElementScope, ".total_gain");
					TotalOffsets[valueElementIdx] = parseFloat(cfg, PIDName, currentElementScope, ".total_offset");
		
					// Parse number of ranges
					numRanges[valueElementIdx]= parseInt(cfg, PIDName, currentElementScope, ".num_ranges");
					
					cout << "Value type element has " << numRanges[valueElementIdx] << "ranges" << endl;
						
					// Allocate all double-pointers for current element range specific attributes now that number of ranges is known
					rangeScalings[valueElementIdx] = new float[numRanges[valueElementIdx]];
					rangeStarts[valueElementIdx] = new float[numRanges[valueElementIdx]];
					rangeStops[valueElementIdx] = new float[numRanges[valueElementIdx]];
					EngUnits[valueElementIdx] = new string[numRanges[valueElementIdx]];
						
					// Parse attributes of each range
					for(int currentParseRange = 1; currentParseRange<=numRanges[valueElementIdx]; currentParseRange++) {
						string currentRangeScope = "range" + to_string(currentParseRange);
						rangeScalings[valueElementIdx][currentParseRange-1] = parseFloat(cfg, PIDName +"."+ currentElementScope, currentRangeScope, ".scaling");
						rangeStarts[valueElementIdx][currentParseRange-1] = parseFloat(cfg, PIDName + "." + currentElementScope, currentRangeScope, ".range_start");
						rangeStops[valueElementIdx][currentParseRange-1] = parseFloat(cfg, PIDName + "." + currentElementScope, currentRangeScope, ".range_stop");
						EngUnits[valueElementIdx][currentParseRange-1] = parseString(cfg, PIDName + "." + currentElementScope, currentRangeScope, ".eng_units");
					}
					valueElementIdx++;
				} // End of value element parsing
				
				// Value type PID element configuration
				if(elementType.compare("bit-encoded") == 0) {									// Value type PID element configuration
					cout << "Bit-encoded type element being parsed!" << endl;
					numBitEncodedBits[bitEncodedElementIdx] = parseInt(cfg, PIDName, currentElementScope, ".num_bits");
					bitNames[bitEncodedElementIdx] = new string[numBitEncodedBits[bitEncodedElementIdx]];
					bit0States[bitEncodedElementIdx] = new string[numBitEncodedBits[bitEncodedElementIdx]];
					bit1States[bitEncodedElementIdx] = new string[numBitEncodedBits[bitEncodedElementIdx]];
					bitLabels[bitEncodedElementIdx] = new string[numBitEncodedBits[bitEncodedElementIdx]];
					
					string startBitString =  parseString(cfg, PIDName, currentElementScope, ".start_bit");
					
					int totalDataBits = numDataBytes * 8; 							// Total number of data bits in entire PID
					int byteNumber = (int)startBitString[0] - 65; 				// ASCII 'A" is 65 -> 0, 'B'->1
					int bitNumber = (int )startBitString[1] - 48;
					
					bitEncodedStartBits[bitEncodedElementIdx] = totalDataBits - 8 * byteNumber - (7 - bitNumber) - 1;
					
					cout << "Start bit " << startBitString << " corresponds with data bit position " << bitEncodedStartBits[bitEncodedElementIdx] << endl;
					
					char start[] = {startBitString[0], startBitString[1], 0};			// null-terminated character arrat for 
					for(int idx = 0; idx < numBitEncodedBits[bitEncodedElementIdx]; idx++) {
						cout << "Parse " << start << endl;

						bitNames[bitEncodedElementIdx][idx] = parseString(cfg, PIDName + "." + currentElementScope, string(start) + "_name");
						bit0States[bitEncodedElementIdx][idx] = parseString(cfg, PIDName + "." + currentElementScope, string(start) + "_0state");
						bit1States[bitEncodedElementIdx][idx] = parseString(cfg, PIDName + "." + currentElementScope, string(start) + "_1state");
					
						
						// Modify start prefix (bit string)
						if(start[1] == '0') {
							start[0]++;
							start[1] = '7';
						}
						else {
							start[1]--;
						}
					}
					
					
					
					bitEncodedElementIdx++;
				} // End of bit-encoded  element parsing 
				
				
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


string PID::getCommand(void) {							// Get command (to issue to ELM)
	return "";
}

				
void PID::update (string serialData, uint64_t updateTime) {				// Update method (serial data, time)
	cout << "PID update called for: " << shortName << endl;
	cout << "  With " << serialData << endl;
	currentTime = updateTime;
	uint64_t timeDelta = currentTime - lastTime;
	lastTime = currentTime;
	updateRate = 1000000./timeDelta;
	
	// Rework validation to define expected response in config file
	string responseID = id;																
	responseID[0] = '4';
	if(!serialData.empty()){
		std::string::iterator end_pos = std::remove(serialData.begin(), serialData.end(), ' ');		
		serialData.erase(end_pos, serialData.end());		// Remove spaces from serial data
		size_t found = serialData.find(responseID);
		if(found != std::string::npos) {
			
			string dataByteString = serialData.substr(found+4, numDataBytes*2);
			//cout << "Data Byte String - " << dataByteString << endl;
			//cout << "num data bytes: " << numDataBytes << endl;
			
			int valueElementIdx = 0;
			int bitEncodedElementIdx = 0;
			int enumElementIdx = 0;

			// Update each element in PID
			for(int currentElement = 0; currentElement < numElements; currentElement++) {
				
				// Value element update
				if(elementTypes[currentElement].compare("value") == 0) {
						int startBytePosition = (int)valueStartBytes[valueElementIdx] - 65; 				// ASCII 'A" is 65 -> 0, 'B'->1
						
						cout << "Start byte " << valueStartBytes[valueElementIdx] << " -> " << startBytePosition << endl;
						values[valueElementIdx] = 0;
						for(int i = 0; i < numValueBytes[valueElementIdx]; i++) {
							cout << "byte gain " << byteGains[valueElementIdx][i] << endl;
							cout << "byte offset " << byteOffsets[valueElementIdx][i] << endl;
							cout << " substring " <<  dataByteString.substr(2*(i+startBytePosition), 2).c_str() << endl;
							cout << "stroutl " << strtoul(dataByteString.substr(2*(i+startBytePosition), 2).c_str(), NULL, 16) << endl;
							values[valueElementIdx] += (byteGains[valueElementIdx][i]*strtoul(dataByteString.substr(2*(i+startBytePosition), 2).c_str(), NULL, 16) + byteOffsets[valueElementIdx][i]);
							cout << "value before total " << values[valueElementIdx]  << endl;
						}
						cout << "TotalGain " << TotalGains[valueElementIdx] << endl;
						cout << "TotalOffset " << TotalOffsets[valueElementIdx] << endl;
						values[valueElementIdx] = (values[valueElementIdx]*TotalGains[valueElementIdx]) + TotalOffsets[valueElementIdx];
						cout << " Value calculated to be: " << values[valueElementIdx] <<endl;
						valueElementIdx++;
				} // End updating value element
				
				
				// Bit encoded element update
				if(elementTypes[currentElement].compare("bit-encoded") == 0) {
					cout << "updating bit-encoded element" << endl;
					cout << "  substring: " << dataByteString.substr(0, 2*numDataBytes) << endl;
					payload =  strtoul(dataByteString.substr(0, 2*numDataBytes).c_str(), NULL, 16);
				} // End updating bit-encoded element
				
			} // End updating all elements
				
		}
	}
}


// Get element value of "value" type
float PID::getValue(string elementName) {
	// loop through PID elements, return a
	int valueElementIdx = 0;
	float result = NAN;
	for(int elementIdx = 0; elementIdx < numElements; elementIdx++) {
		if(elementNames[elementIdx].compare(elementName) == 0) {
			result = values[valueElementIdx];
			break;
		}
		else if(elementTypes[elementIdx].compare("value") == 0) {
			valueElementIdx++;
		}
	}
	if(isnan(result)) {
		cout << "getValue was unable to find element " << elementName << " in PID " << id << endl;
	}
	return result;
}
			/*
		
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
		}
	}
}			// END PID UPDATE





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



*/
// Bit-encoded functions

int PID::getBitsEncoded(string elementName) {
	// loop through PID elements
	int bitEncodedElementIdx = 0;
	int result = 0;
	for(int elementIdx = 0; elementIdx < numElements; elementIdx++) {
		if(elementNames[elementIdx].compare(elementName) == 0) {
			result = numBitEncodedBits[bitEncodedElementIdx];
			break;
		}
		else if(elementTypes[elementIdx].compare("bit-encoded") == 0) {
			bitEncodedElementIdx++;
		}
	}
	return result;
}

string PID::getBitName(string elementName, int bitIndex) {
	// loop through PID elements
	int bitEncodedElementIdx = 0;
	string name = "";
	for(int elementIdx = 0; elementIdx < numElements; elementIdx++) {
		if(elementNames[elementIdx].compare(elementName) == 0) {
			name = bitNames[bitEncodedElementIdx][bitIndex];
			break;
		}
		else if(elementTypes[elementIdx].compare("bit-encoded") == 0) {
			bitEncodedElementIdx++;
		}
	}
	return name;
}

bool PID::getBitValue(string elementName, int bitIndex) {
	// loop through PID elements
	int bitEncodedElementIdx = 0;
	bool result = false;
	for(int elementIdx = 0; elementIdx < numElements; elementIdx++) {
		if(elementNames[elementIdx].compare(elementName) == 0) {
			int start = bitEncodedStartBits[bitEncodedElementIdx];
			result = ((payload >> (31 - start - bitIndex) ) &  1);
			break;
		}
		else if(elementTypes[elementIdx].compare("bit-encoded") == 0) {
			bitEncodedElementIdx++;
		}
	}
}


bool PID::getBitNameValue(string elementName, string bitName) {
	bool val = false;
	int bitEncodedElementIdx = 0;
	for(int elementIdx = 0; elementIdx < numElements; elementIdx++) {
		if(elementNames[elementIdx].compare(elementName) == 0) {
			for(int i = 0; i < numBitEncodedBits[bitEncodedElementIdx]; i++) {
				if(bitName.compare(bitNames[bitEncodedElementIdx][i]) == 0) {
					val = getBitValue(elementName, i);
					break;
				}
			}
		}
		else if(elementTypes[elementIdx].compare("bit-encoded") == 0) {
			bitEncodedElementIdx++;
		}
	}
	return val;
}
	
string PID::getBitNameState(string elementName, string bitName) {
	string state = "";
	int bitEncodedElementIdx = 0;
	for(int elementIdx = 0; elementIdx < numElements; elementIdx++) {
		if(elementNames[elementIdx].compare(elementName) == 0) {
			for(int i = 0; i < numBitEncodedBits[bitEncodedElementIdx]; i++) {
				if(bitName.compare(bitNames[bitEncodedElementIdx][i]) == 0) {
					state = getBitState(elementName, i);
					break;
				}
			}
		}
		else if(elementTypes[elementIdx].compare("bit-encoded") == 0) {
			bitEncodedElementIdx++;
		}
	return state;
	}
}

string PID::getBitState(string elementName, int pos) {
	string state = "";
	int bitEncodedElementIdx = 0;
	for(int elementIdx = 0; elementIdx < numElements; elementIdx++) {
		if(elementNames[elementIdx].compare(elementName) == 0) {
			bool val = getBitValue(elementName, pos);
			if (val) {
				state = bit1States[bitEncodedElementIdx][pos];
			}
			else {
				state = bit0States[bitEncodedElementIdx][pos];
			}
		}
		else if(elementTypes[elementIdx].compare("bit-encoded") == 0) {
			bitEncodedElementIdx++;
		}
	}	
	return state;
}

string PID::getBitLabel(string elementName, int bitIndex) {
string label = "";
	int bitEncodedElementIdx = 0;
	for(int elementIdx = 0; elementIdx < numElements; elementIdx++) {
		if(elementNames[elementIdx].compare(elementName) == 0) {
			label = bitLabels[bitEncodedElementIdx][bitIndex];
		}
		else if(elementTypes[elementIdx].compare("bit-encoded") == 0) {
			bitEncodedElementIdx++;
		}
	}	
	return label;
}


