#ifndef TEXTVIEW_H
#define TEXTVIEW_H

#include "TouchableObject.h"

/********************************/
/*		TextView Class			*/
/********************************/

using namespace std;

class TextView : public TouchableObject {
private:
	string textViewIdentifier;

	int width, height;
	int borderWidth;
	int centerX, centerY;
	int bottomLeftX, bottomLeftY;
	int rectWidth, rectHeight;
	int cornerRadius;
	int fontSize;
	int textPadding;
	int numLines;
	string* textViewData;

	// Line identifier driven operation
	string* lineIdentifiers;
	string* lineLabels;
	string* lineText;

	float borderColor[4];
	float borderColorAlpha;
	float backgroundColor[4];
	float backgroundColorAlpha;
	float textColor[4];
	float textColorAlpha;
	int currentLine;

	float** lineColors;

public:
	TextView(int, int, int, int, string);			// TextView Constructor: center X, center Y, width, height, identifier
	void configure(string);
	void update();
	void addNewLine(string);
	void addNewLine(string, float*);
	void clearLastLine(void);
	void clear(void);


	void addNewLineIdentifier(string, string);		// Add Line identifier, line label
	void setLineIdentiferText(string, string);		// Set line text (identifier, new text)
	

	void update(touch_t){}
};

#endif
