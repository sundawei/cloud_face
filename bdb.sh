g++ rv2db.cpp `mysql_config --cflags --libs` -I/usr/local/log4cxx/include -I/opt/Release-EyeFaceSDK-v3.12.0-CentOS-6.6-x86_64-hasp/eyefacesdk/include/ -I/usr/local/include/opencv -lopencv_core -lopencv_imgproc -lopencv_highgui -lqpidclient -lqpidmessaging -fopenmp -llog4cxx -g -Wall -ldl -lpthread -Wall -O3 -fomit-frame-pointer -ffast-math -fPIC -DCPP -o rv2db



