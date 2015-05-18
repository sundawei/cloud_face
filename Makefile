# Compiler setup
CC = gcc
CXX = g++

#CFLAGS = -Wall -O3 -fomit-frame-pointer -ffast-math -fPIC -DHASP_SECURE -DEYEFACE_FULL
CFLAGS = -Wall -g -ffast-math -fPIC -DCPP -DHASP_SECURE -fmessage-length=0 -fopenmp

LDFLAGS = -ldl -pthread -L/usr/local/lib -lopencv_highgui -lopencv_core -lqpidclient -lpthread -lqpidmessaging -lcurl -lopencv_imgproc -lopencv_highgui -lopencv_ml -lopencv_video -lopencv_features2d -lopencv_calib3d -lopencv_objdetect -lopencv_contrib -lopencv_legacy -lopencv_flann -luuid -llog4cxx

# Additional directories to be searched for header files
INCLUDES =  -I../../eyefacesdk/include -I/usr/local/include -I/usr/local/log4cxx/include -I/usr/local/include/opencv -I/usr/include/curl

INSTLIBDIR = .

CFILES = 
CPPFILES = 

CPPMAIN = FaceUploader_self3.cpp
EXECUTABLE= FaceUploader_self3

# ------------------------------------------------
OBJ = $(patsubst %.c,%.o,$(CFILES))
H = $(patsubst %.c,%.h,$(CFILES))
OBJCPP = $(patsubst %.cpp,%.o,$(CPPFILES))
HCPP = $(patsubst %.cpp,%.h,$(CPPFILES))

OBJMAIN = $(patsubst %.cpp,%.o,$(CPPMAIN))

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)  $(OBJCPP)  $(OBJMAIN)
	$(CXX) -o $(INSTLIBDIR)/$(EXECUTABLE) $(OBJ) $(OBJCPP) $(OBJMAIN) $(LDFLAGS)

%.o : %.c %.h 
	$(CC) ${CFLAGS} $(INCLUDES) -o $*.o -c $*.c
%.o : %.c 
	$(CC) ${CFLAGS} $(INCLUDES) -o $*.o -c $*.c
%.o : %.cpp %.h 
	$(CXX) ${CFLAGS} $(INCLUDES) -o $*.o -c $*.cpp
%.o : %.cpp
	$(CXX) ${CFLAGS} $(INCLUDES) -o $*.o -c $*.cpp


GARBAGE = core *~

.PHONY:clean
clean:
	$(RM) $(EXECUTABLE) $(EXECUTABLE_HASP) $(OBJ) $(OBJCPP) $(OBJMAIN)  $(OBJMAIN_HASP) $(GARBAGE)


