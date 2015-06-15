g++ -Wall -I/usr/local/log4cxx/include -I/usr/local/include/opencv -I/usr/include/curl -I../include -O0 -g3 -Wall -c -fmessage-length=0 -fopenmp -o FaceUploader_self.o FaceUploader_self3.cpp
g++ -L../lib/ -o FaceUpLoader_self3  ./FaceUploader_self.o   -lopencv_core -lqpidclient -lpthread -lqpidmessaging -lcurl -lopencv_imgproc -lopencv_highgui -lopencv_ml -lopencv_video -lopencv_features2d -lopencv_calib3d -lopencv_objdetect -lopencv_contrib -lopencv_legacy -lopencv_flann -luuid -llog4cxx -Wall



