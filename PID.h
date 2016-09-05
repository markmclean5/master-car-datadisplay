#ifndef PID_H
#define PID_H

#include <string>

/**************************************************************************************************************
 * PID Class
 * Data class for processing OBD2 PID data received and processing the data into engineering units.
 *
 * Provides:	Last raw datum recieved
 *				Weighted moving average of data using provided coefficents and lag
 *				Simple moving average of data using provided lag
 *				Capability to set multiple ranges of data output in different eng. units
 *					Note: Ranges must not overlap. Example: For boost gauge processing,
 *					input data is recieved in PSI. Positive input data is output in "PSI",
 *					and negative pressure data is scaled and reported in "inHg".
 *
 **************************************************************************************************************/


class PID {
private:	// Class Private properties

	std::string fullName;
	std::string shortName;
	std::string id;
	
	// Class properties
	bool debug;
	
	// Add more timing replated properties
	uint64_t lastTime;
	uint64_t currentTime;
	uint64_t lastUpdateTime;
	float updateRate;			// The rate at which PID.update() is being called 


	
	int numDataBytes;

	int numElements;			// Total number of data elements in the ("num_elements" in config file)
	int numValueElements;		// Number of value elements in the PID (type "value" in config file)
	int numBitEncodedElements;	// Number of bit-encoded elements in the PID (type "bit-encoded" in config file)
	int numEnumeratedElements;	// Number of enumerated-elements in the PID (type "enumerated" in config file)
	string* types;				// Type of each element in the PID


	// Attributes of "value" type element
	char* valueStartBytes;			// Byte at which each element starts
	int* numValueBytes;			// Number of bytes for each element
	float* supportedMinVals;	// Minumum supported value for each element	
	float*supportedMaxVal;		// Maximum supported value for each element

	float** byteGains;			// gain values for each byte of each element
	float** byteOffsets;		// offset values for each byte of each element
	float* TotalGains;			// total gain values for each element
	float* TotalOffsets;		// total offset values for each element
	int* numRanges;				// number of ranges for each element

	// "value" type element range element
	float** rangeScalings;		// scaling value of every range of each element
	float** rangeStarts;		// start value of every range of each element
	float** rangeStops;			// stop value of every range of each element
	std::string** EngUnits;		// engineering units of every range of each element
	
	int* currentRanges;			// current range number for each element
	int* lastRanges;			// last range number for each element


	// Attributes of "bit-encoded" type element
	uint32_t* bitValues;		// 32 bit storage for bit states (entire PID, all elements)

	int* numBitEncodedBits;		// start bit position for each element
	int* bitEncodedStartBits;	// 

	std::string** bitNames;		// bit names for each bit of each element
	std::string** bit0States;	// bit '0' state names for each bit of each element
	std::string** bit1States;	// bit '1' state names for each bit of eahc element
	std::string** bitLabels;	// bit labels for each bit of each element (display)
	
	// Attributes of "enumerated" type element
	int* numEnums;				// Number of enumerations for each element
	int* enumStartBits;			// Start bit positions for each element
	int* numEnumBits;			// number of bits for each element
	int* numVals;				// Number of enumeration values for each element
	int** enumVals;				// array of values for each enumeration of each element
	std::string ** enumStates;	// array of states for each enumeration of each element
	


public:		// Class members
	PID(std::string); 							// PID constructor, accepts name of identifer to find PID configuration)

	// Get information about the PID and data elements
	int getNumPIDElements(void);							// Get the number of elements contained in the PID
	string getPIDElementName(int);							// Get the (name string) of an element (#)
	string getPIDElementType(int);							// Get the (type string) of an element (#)

	// Get data from value elements
	float getValue(std::string);							// Get raw datum for the provided element name
	std::string getValueEngUnits(std::string);				// Get engineering units for the provided element name

	// Get data from bit-encoded elements
	int getBitsEncoded(std::string);						// Get number of bits of a provided element name
	std::string getBitName(std::string, int);				// Get (name string) of provided bit (#) of provided elenment
	bool getBitNameValue(std::string, std::string);			// Get value (t/f) of provided bit name of provided element name
	std::string getBitNameState(std::string, std::string);	// Get (state string) of provided bit name of provided element name
	bool getBitValue(std::string, int);						// Get value (t/f) of provided bit (#) of provided element name
	std::string getBitState(std::string, int);				// Get (state string) of provided bit (#) of provided element name
	std::string getBitLabel(std::string, int);				// Get (label string) of provided bit (#) of provided element
	
	// Get data from enumerated elements
	std::string getEnumName(std::string);					// Get (name string) of the provided element name
	int getEnumValue(std::string);							// Get the enumeration value (#) of the provided element name
	std::string getEnumState(std::string);					// Get the enumeration (state string) of the provided element name

	// General PID operation
	float getRawUpdateRate(void);							// Get update rate (Hz, between update() calls)
	std::string getCommand(void);							// Get command (to issue to ELM)
	void update(std::string, uint64_t);						// Update method (serial data, time)

	
	std::string command;

	// Datalinks - count number of elements for each mode which rely on data from this PID
	int dashboard_datalinks;
	int plot_datalinks;
	int log_datalinks;



private:	// Class private members

	void configure(std::string);							// PID configure method

};
#endif