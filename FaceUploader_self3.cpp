/*
* FaceUploader.cpp
*
*  Created on: Jun 5, 2013
*      Author: sun
*/
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <memory.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <cv.h>
#include <highgui.h>
#include <curl.h>
#include <pthread.h>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <stdlib.h>


#include <sstream>
#include <uuid/uuid.h>

#include <log4cxx/logger.h>    
#include <log4cxx/logstring.h> 
#include <log4cxx/propertyconfigurator.h> 

using namespace log4cxx; 
using namespace qpid::messaging;
using namespace qpid::types;

using std::stringstream;
using std::string;
using namespace std;

#define _TCHAR char
#define _tprintf printf
#define _tfopen fopen
#define _fgetts fgets
#define _tcsstr strstr
#define _T(x) x

#ifndef _MAX_PATH
#define _MAX_PATH       1024
#endif


#define   IPCKEY 0x88F
#define   MSG_FILE "./msgq" 
#define   BUFFER 1024*1024*5 
#define   PERM S_IRUSR|S_IWUSR 


vector<Message> msgQ;
//change send msg by system v

key_t v_key; 
int v_msgid; 
struct msgtype { 
    long mtype; 
    char buffer[BUFFER]; 
};
struct RealMsg{
	long msglen;
	char* buffer;
};

struct msgtype SMSG;
struct msgtype RMSG;


pthread_mutex_t file_mutex; 

char FBUFFER[1024*1024*5];

int WF_count=0;

char place[64] = {0};
char pos[64] = {0};
char camname[64] = {0};
char mjpeg[64] = {0};
char qurl[64] = {0};
char qaddress[64] = {0};
char workmode[64] = {0};
char m0[64] = {0};
char m1[64] = {0};
char m2[64] = {0};

using namespace cv;

LoggerPtr logger=0;

struct MemoryStruct {
	char *memory;
	size_t size;
};

struct SendParam {
	string a;
	string b;
	string c;
	IplImage* img;
};





typedef struct _GUID
{
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID, UUID;
GUID CreateGuid()
{
    GUID guid;
#ifdef WIN32
    CoCreateGuid(&guid);
#else
    uuid_generate(reinterpret_cast<unsigned char *>(&guid));
#endif
    return guid;
}
std::string GuidToString(const GUID &guid)
{
    char buf[64] = {0};
#ifdef __GNUC__
    snprintf(
#else // MSVC
    _snprintf_s(
#endif
                buf,
                sizeof(buf),
                 "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                guid.Data1, guid.Data2, guid.Data3,
                guid.Data4[0], guid.Data4[1],
                guid.Data4[2], guid.Data4[3],
                guid.Data4[4], guid.Data4[5],
                guid.Data4[6], guid.Data4[7]);
        return std::string(buf);
}



void EncodeImg2Jpg(IplImage* img,char* buffer,int & len)
{
	vector<int> p;
	p.push_back(CV_IMWRITE_JPEG_QUALITY);
	p.push_back(100);
	vector<unsigned char> buf;
	cv::imencode(".jpg", (Mat)img, buf, p);

	len = buf.size();
	memcpy(buffer,buf.data(),buf.size());

	std::vector<unsigned char>().swap(buf);
}



int sneFace2Qpid(Message message)
{
	int ret = 0;
	std::string connectionOptions =  "";

	Connection connection(qurl, connectionOptions);
	connection.setOption("reconnect", true);
	try {
		connection.open();
		Session session = connection.createSession();
		Sender sender = session.createSender(qaddress);
		sender.send(message, true);
		connection.close();
		LOG4CXX_TRACE(logger,"send a message"); 
		ret = 1;

	} catch(const std::exception& error) {
		std::cout << error.what() << std::endl;
		LOG4CXX_TRACE(logger,error.what()); 
		connection.close();
		ret = 0;
	}
	return ret;
}
string GetDateString()
{
	time_t now;
	struct tm *tm_now;
	char    sdatetime[20];

	time(&now);
	tm_now = localtime(&now);
	strftime(sdatetime, 20, "./%Y-%m-%d", tm_now);
	return std::string(sdatetime);
}

void  *SendMessageFormFile(void *arg)
{
	while(1)
	{
		sleep(1);
		pthread_mutex_lock(&file_mutex);
		if(msgQ.size()>0)
		{
			for(int i=0;i<msgQ.size();i++)
			{
				sneFace2Qpid(msgQ.at(i));
			}
			vector<Message>().swap(msgQ);
		}
		pthread_mutex_unlock(&file_mutex);

	}
	return 0;

}


void SaveMessage2Disk(char* buffer, int length)
{
	LOG4CXX_TRACE(logger,"encode and send msg ");
	Variant::Map content;
	content["place"] = place;
	content["pos"] = pos;
	content["camname"] = camname;
	content["workmode"] = workmode;
	content["m0"] = m0;
	content["m1"] = m1;
	content["m2"] = m2;
	content["int"] = time(0);
	string spic;
	spic.assign(buffer,length);
	content["picture"] = spic;
	content["uuid"] = GuidToString(CreateGuid());	
	Message message;
	encode(content, message);

	pthread_mutex_lock(&file_mutex);
	if(msgQ.size()<50)
	msgQ.push_back(message);
	pthread_mutex_unlock(&file_mutex);

}
int SendFace2Qpid(char* buffer,int length)
{	
		SaveMessage2Disk(buffer,length);
		return 1;
}

void  *SendInfo(void *arg)
{
	SendParam* ppm = (SendParam*)arg;
	char* SendBuffer=new char[1024*1024*4];
	int piclen=0;
	EncodeImg2Jpg(ppm->img,SendBuffer,piclen);
	printf("[sam]piclen = %d\n",piclen);
	int ret = SendFace2Qpid(SendBuffer,piclen);
	while(ret==0)
	{
		ret = SendFace2Qpid(SendBuffer,piclen);
	}

	delete [] SendBuffer;
	cvReleaseImage(&ppm->img);
	delete [] (char*)arg;
	return 0;

}

void cropImage(IplImage* src,IplImage* & dstimg,CvRect r)
{
	dstimg = cvCreateImage(cvSize(r.width,r.height),src->depth,src->nChannels);
	(((Mat)(src))(r)).convertTo( ((Mat)(dstimg)), ((Mat&)(dstimg)).type(),1,0);
}
const char* pFormat = "%Y-%m-%d-%H-%M-%S";
int ia=0;
void Enroll(int argc, _TCHAR* argv[],IplImage* img0,char* jpgdata,int jpglen)
{

			CvMemStorage* storage=0;
			CvHaarClassifierCascade* cascade=0;
			char cascadename[100]="./haarcascade_frontalface_alt.xml";
			cascade = (CvHaarClassifierCascade*)cvLoad(cascadename,0,0,0);
			storage = cvCreateMemStorage(0);
			CvSeq* faces;

			IplImage *frame_ys_gray=cvCreateImage(cvGetSize(img0),IPL_DEPTH_8U,1);
			cvCvtColor(img0,frame_ys_gray,CV_BGR2GRAY);

			faces = cvHaarDetectObjects(frame_ys_gray,cascade,storage,1.1,3,0,cvSize(100,100));


			for(short i=0;i<(faces?faces->total:0);i++)
			{
				
				CvRect* rect = (CvRect*)cvGetSeqElem(faces,i);
				IplImage *aface;
				//CvRect rnew=cvRect(0,0,img0->width,img0->height);
				int newx,newy,neww,newh;
				int w=rect->width;
				int h=rect->height;
				int ws=w;
				int hs=h;
				newx=rect->x-ws*0.5;
				newy=rect->y-hs*0.7;
				neww=rect->width+ws;
				newh=rect->height+hs;

				if(newx<0)
					newx=0;
				if(newy<0)
					newy=0;

				if(newx+neww>img0->width-1)
				{
					neww=neww-(img0->width-neww-2);
				}

				if(newy+newh>img0->height-1)
				{
					newh=newh-(img0->height-newh-2);
				}

				CvRect rnew=cvRect(newx,newy,neww,newh);



				cropImage(img0,aface,rnew);
				char* pq=new char[sizeof(SendParam)];
				SendParam* op = (SendParam*)pq;
				op->img =  cvCloneImage(aface);
				pthread_t  tid;
				pthread_create( &tid, NULL, SendInfo, op );
				//SendInfo(op);
			//	char dbs[50]={0};
			//	sprintf(dbs,"./imgd/%05d.jpg",ia++);
			//	cvSaveImage(dbs,aface);
				cvReleaseImage(&aface);
			}
			cvReleaseImage(&frame_ys_gray);

			cvReleaseMemStorage(&storage);
			cvReleaseHaarClassifierCascade(&cascade);
}

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL) {
		/* out of memory! */
		LOG4CXX_TRACE(logger,"not enough memory (realloc returned NULL)");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}


void getPic(char* pic,long & picsize)
{
	CURL *curl_handle;
	CURLcode res;

	struct MemoryStruct chunk;

	chunk.memory = (char*)malloc(1); 
	chunk.size = 0;    

	curl_global_init(CURL_GLOBAL_ALL);


	curl_handle = curl_easy_init();


	curl_easy_setopt(curl_handle, CURLOPT_URL, mjpeg);
	curl_easy_setopt(curl_handle, CURLOPT_FORBID_REUSE, 1);
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L);




	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);


	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	res = curl_easy_perform(curl_handle);

	if(res != CURLE_OK) {
		LOG4CXX_TRACE(logger,"curl_easy_perform() failed: "<<curl_easy_strerror(res));
		picsize = 0;
	}
	else {
		memcpy(pic,chunk.memory,chunk.size);
		picsize = chunk.size;
	}
	curl_easy_cleanup(curl_handle);

	if(chunk.memory)
		free(chunk.memory);
	chunk.memory=0;
	curl_global_cleanup();

	return ;
}
char buffer[1024*1024*10];
long nsize=0;

void  *GetUrlPic(void *arg)
{
	while(1)
	{
		getPic(buffer,nsize);
		if(nsize>0)
		{
			std::vector<char> data1(buffer, buffer + nsize);
			cv::Mat imgMat = cv::imdecode(Mat(data1), CV_LOAD_IMAGE_COLOR);
			IplImage img=imgMat;
			Enroll(0,0,&img,buffer,nsize);
			data1.clear();
			std::vector<char>().swap(data1);
		}
		printf("workmode %d\n",atoi(workmode));
		int sleepm=atoi(workmode);
		if(sleepm<0)
			sleepm=500;
		usleep(1000*sleepm);
		if(cvWaitKey(20)==27)
			break;
	}
	cvDestroyAllWindows();

	return 0;
}



int main( int argc, char** argv )
{
	PropertyConfigurator::configure("ta_faceuploader_logconfig.cfg"); 
	logger = Logger::getLogger("Trace_FaceUpLoader"); 
	pthread_mutex_init(&file_mutex,NULL); 
	if(argc!=10)
	{
		LOG4CXX_TRACE(logger,"pid = " << getpid() << "bad start args argc= "<<argc); 
	}
	else
	{
		strcpy(place,argv[0]);
		strcpy(pos,argv[1]);
		strcpy(camname,argv[2]);
		strcpy(mjpeg,argv[3]);
		strcpy(qurl,argv[4]);
		strcpy(qaddress,argv[5]);
		strcpy(workmode,argv[6]);
		strcpy(m0,argv[7]);
		strcpy(m1,argv[8]);
		strcpy(m2,argv[9]);

	}
	pthread_t  tid,tidsnd;
	int  ret;
	ret = pthread_create( &tid, NULL, GetUrlPic, 0 );
	ret = pthread_create( &tidsnd, NULL, SendMessageFormFile, 0);
	while(1)
	{
		sleep(10000000);
	}
	pthread_mutex_destroy(&file_mutex);
	return 0;
	
}
