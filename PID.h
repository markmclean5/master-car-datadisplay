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
	// Setters (call before update!!!!)

	PID(std::string); 							// PID constructor, accepts name of identifer to find PID configuration)
	void setDebugMode(bool);					

	void setCharTags(char, char);				// Set serial stream start and stop characters		(start char, stop char)
	void setStreamScaling(float);						// Set stream scaling (convert to eng. units)		(set)
	void setRangeLimits(float,float, int);		// Set data range limits 		(min, max, range #)
	void setRangeScaling(float, int);			// Set range scaling, range
	void setEngUnits(std::string, int);				// Set engineering unit string, range


	// Weighted Moving Average Setters
	void setWeightedMALag(int, int);			// Weighted MA lag, range
	void setWeightedMACoeffs(float*, int);		// Weighted MA coeffs, range

	// Simple Moving Average Setter
	void setSimpleMALag(int, int);				// Simple MA lag, range

	// Setters readout update frequency
	void setReadoutFreq(int);					

	
	// Getters for DataStream data
	float getRawDatum(void);
	float getWeightedMADatum(void);
	float getSimpleMADatum(void);
	float getReadoutDatum(void);
	std::string getEngUnits(void);


	// Getters for DataStream timing info
	float getRawUpdateRate(void);
	float getReadoutUpdateRate(void);

	// Update function
	void update(std::string, uint64_t);				// Update method									(serial stream, time)
	std::string getCommand(void);
	std::string command;


	// Bit-encoded functions
	bool getBitNameValue(std::string);
	std::string getBitNameState(std::string);
	bool getBitPositionValue(int);
	std::string getBitPositionState(int);
	std::string getBitPositionName(int);
	std::string getBitPositionLabel(int);
	int getNumBits(void);

	// Datalinks - count number of elements for each mode which rely on data from this PID
	int dashboard_datalinks;
	int plot_datalinks;
	int log_datalinks;



private:	// Class private members

	void configure(std::string);						// PID configure method

};
#endif