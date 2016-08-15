CXX=distcc arm-linux-gnueabihf-g++ -std=gnu++0x $(INCLUDEFLAGS) -g
INCLUDEFLAGS=-I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I.. -I/home/pi/config4cpp/include
LIBFLAGS=-L/opt/vc/lib -L/opt/vc/lib -L/home/pi/config4cpp/lib -lGLESv2 -lEGL -lbcm_host -lpthread  -ljpeg
objects = touchscreen.o Gauge.o TouchableObject.o DisplayableObject.o DataStream.o Button.o TextView.o Menu.o Serial.o parsingUtilities.o PID.o modeManager.o DisplayObjectManager.o PIDVectorManager.o ConnectionManager.o main.o

all: $(objects) car-datadisplay

main.o: main.cpp

Menu.o: Menu.cpp
TextView.o: TextView.cpp
Button.o: Button.cpp
DataStream.o: DataStream.cpp
Gauge.o: Gauge.cpp
Serial.o: Serial.cpp
TouchableObject.o: TouchableObject.cpp
DisplayableObject.o: DisplayableObject.cpp
touchscreen.o: touchscreen.cpp
parsingUtilities.o: parsingUtilities.cpp
PID.o: PID.cpp
modeManager.o: modeManager.cpp
DisplayObjectManager.o: DisplayObjectManager.cpp
PIDVectorManager.o: PIDVectorManager.cpp
ConnectionManager.o: ConnectionManager.cpp


car-datadisplay: car-datadisplay $(objects)
	g++ -Wall -std=gnu++0x $(LIBFLAGS) -o car-datadisplay $(objects) ../openvg/libshapes.o ../openvg/oglinit.o -lbcm2835 -lconfig4cpp
	rm *.o
