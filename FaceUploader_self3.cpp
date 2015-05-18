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
#include <sys/time.h>
#include <opencv/cv.h>
#include <opencv2/highgui/highgui.hpp>
#ifdef HASP_SECURE
  #include "eyeface-hasp.h"
#endif
#include "EyeFace.h"

#if defined(WIN32) || defined(_WIN32) || defined(_DLL) || defined(_WINDOWS_) || defined(_WINDLL)
  #define MODULE_DIR   (char*)"..\\..\\eyefacesdk"
  #define LOG_DIR      (char*)".\\logs"
  #define INI_DIR      (char*)"..\\..\\eyefacesdk"
  #define INI_FILENAME (char*)"config.ini"
#else
  #define MODULE_DIR   (char*)"../../eyefacesdk/"
  #define LOG_DIR      (char*)"./logs/"
  #define INI_DIR      (char*)"../../eyefacesdk/"
  #define INI_FILENAME (char*)"config.ini"
#endif

#define NOMINMAX

#if defined(WIN32) || defined(_WIN32) || defined(_DLL) || defined(_WINDOWS_) || defined(_WINDLL)
    char lib_name[] = "..\\..\\eyefacesdk\\lib\\EyeFace.dll";
#else 
    #if defined(x86_32) || defined(__i386__)
        char lib_name[] = "../../eyefacesdk/lib/libeyefacesdk-x86_32.so";
    #else
        char lib_name[] = "../../eyefacesdk/lib/libeyefacesdk-x86_64.so";
    #endif
#endif

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#define VERTICAL_FLIP  0                  
#define HORIZONTAL_FLIP 1                  
#define IMG_WIDTH 640                      
#define IMG_HEIGHT 480                     
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#define _TCHAR char
#define _tprintf printf
#define _tfopen fopen
#define _fgetts fgets
#define _tcsstr strstr
#define _T(x) x
#ifndef _MAX_PATH
#define _MAX_PATH       1024
#endif
#define   BUFFER 1024*1024*15 

using namespace log4cxx; 
using namespace qpid::messaging;
using namespace qpid::types;
using std::stringstream;
using std::string;
using namespace std;
using namespace cv;

int show_fps = false;
int show_landmarks = false;
IplImage* all_faces[10] = {0};
vector<Message> msgQ;
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
LoggerPtr logger=0;
const char* pFormat = "%Y-%m-%d-%H-%M-%S";
int ia=0;
unsigned char * raw_img   = NULL;
IplImage   * grey_image   = NULL;    
void * eyeface_state = NULL;
int iname=1000;
char buffer[1024*1024*10];
long nsize=0;

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
#else
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
    try 
    {
        connection.open();
        Session session = connection.createSession();
        Sender sender = session.createSender(qaddress);
        sender.send(message, true);
        connection.close();
        LOG4CXX_TRACE(logger,"send a message"); 
        ret = 1;
    } 
    catch(const std::exception& error) 
    {
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
    char sdatetime[20];
    time(&now);
    tm_now = localtime(&now);
    strftime(sdatetime, 20, "./%Y-%m-%d", tm_now);
    return std::string(sdatetime);
}

void  *SendMessageFormFile(void *arg)
{
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
    if(msgQ.size() < 50)
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
    char* SendBuffer = new char[1024*1024*4];
    int piclen = 0;
    EncodeImg2Jpg(ppm->img,SendBuffer,piclen);
    printf("[sam]piclen = %d\n",piclen);
    int ret = SendFace2Qpid(SendBuffer,piclen);
    while(ret == 0)
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
ef_shlib_hnd dll_handle;                                                  
fcn_efReadImageUc                fcnEfReadImageUc;
fcn_efFreeImageUc                fcnEfFreeImageUc;
fcn_efInitEyeFace                fcnEfInitEyeFace;
fcn_efFreeEyeFaceState           fcnEfFreeEyeFaceState;
fcn_efClearEyeFaceState          fcnEfClearEyeFaceState;
fcn_efShutdownEyeFace            fcnEfShutdownEyeFace;
fcn_efMain                       fcnEfMain;
fcn_efGetVisualOutput            fcnEfGetVisualOutput;
fcn_efFreeVisualOutput           fcnEfFreeVisualOutput;
fcn_efGetTrackFinalInfoOutput    fcnEfGetTrackFinalInfoOutput;
fcn_efFreeTrackFinalInfoOutput   fcnEfFreeTrackFinalInfoOutput;
fcn_efSetEyeFaceFlagParams       fcnEfSetEyeFaceFlagParams;
fcn_efGetEyeFaceFlagParams       fcnEfGetEyeFaceFlagParams;
fcn_efGetLicenseExpirationDate   fcnEfGetLicenseExpirationDate;
fcn_efHaspGetCurrentLoginKeyId   fcnEfHaspGetCurrentLoginKeyId;
fcn_efHaspGetKeyIdsFromInfo      fcnEfHaspGetKeyIdsFromInfo;
fcn_efHaspGetKeyInfo             fcnEfHaspGetKeyInfo;
fcn_efHaspGetSessionKeyInfo      fcnEfHaspGetSessionKeyInfo;
fcn_efHaspWriteC2VtoFile         fcnEfHaspWriteC2VtoFile;
fcn_efHaspWriteAllC2VtoOneFile   fcnEfHaspWriteAllC2VtoOneFile;
fcn_efHaspActivateV2C            fcnEfHaspActivateV2C;
fcn_efGetConnectionState         fcnEfGetConnectionState;

int loadDll(const char * dll_filename)
{
    EF_OPEN_SHLIB(dll_handle, dll_filename);
    if (!dll_handle)
        return -1;

    // link efReadImageUC() 
    EF_LOAD_SHFCN(fcnEfReadImageUc, fcn_efReadImageUc, dll_handle, "efReadImageUc");
    if (!fcnEfReadImageUc)
        return -2;

    // link efFreeImageUC()
    EF_LOAD_SHFCN(fcnEfFreeImageUc, fcn_efFreeImageUc, dll_handle,  "efFreeImageUc");
    if (!fcnEfFreeImageUc)
        return -3;

    // link efInitEyeFace()
    EF_LOAD_SHFCN(fcnEfInitEyeFace, fcn_efInitEyeFace, dll_handle, "efInitEyeFace");
    if (!fcnEfInitEyeFace)
        return -4;

    // link efShutdownEyeFace()
    EF_LOAD_SHFCN(fcnEfShutdownEyeFace, fcn_efShutdownEyeFace, dll_handle, "efShutdownEyeFace");
    if (!fcnEfShutdownEyeFace)
        return -5;

    // link efFreeEyeFaceState()
    EF_LOAD_SHFCN(fcnEfFreeEyeFaceState, fcn_efFreeEyeFaceState, dll_handle, "efFreeEyeFaceState");
    if (!fcnEfFreeEyeFaceState)
        return -6;

    // link efClearEyeFaceState()
    EF_LOAD_SHFCN(fcnEfClearEyeFaceState, fcn_efClearEyeFaceState, dll_handle, "efClearEyeFaceState");
    if (!fcnEfClearEyeFaceState)
        return -7;

    // ink efMain() 
    EF_LOAD_SHFCN(fcnEfMain, fcn_efMain, dll_handle, "efMain");
    if (!fcnEfMain)
        return -8;

    // link efGetVisualOutput()
    EF_LOAD_SHFCN(fcnEfGetVisualOutput, fcn_efGetVisualOutput, dll_handle, "efGetVisualOutput");
    if (!fcnEfGetVisualOutput)
        return -9;

    // link efFreeVisualOutput()
    EF_LOAD_SHFCN(fcnEfFreeVisualOutput, fcn_efFreeVisualOutput, dll_handle, "efFreeVisualOutput");
    if (!fcnEfFreeVisualOutput)
        return -10;

    // link efGetTrackFinalInfoOutput()
    EF_LOAD_SHFCN(fcnEfGetTrackFinalInfoOutput, fcn_efGetTrackFinalInfoOutput, dll_handle, "efGetTrackFinalInfoOutput");
    if (!fcnEfGetTrackFinalInfoOutput)
        return -11;

    // link efFreeTrackFinalInfoOutput()
    EF_LOAD_SHFCN(fcnEfFreeTrackFinalInfoOutput, fcn_efFreeTrackFinalInfoOutput, dll_handle, "efFreeTrackFinalInfoOutput");
    if (!fcnEfFreeTrackFinalInfoOutput)
        return -12;

    // link efSetEyeFaceFlagParams() 
    EF_LOAD_SHFCN(fcnEfSetEyeFaceFlagParams, fcn_efSetEyeFaceFlagParams, dll_handle, "efSetEyeFaceFlagParams");
    if (!fcnEfSetEyeFaceFlagParams)
        return -13;

    // link efGetEyeFaceFlagParams()
    EF_LOAD_SHFCN(fcnEfGetEyeFaceFlagParams, fcn_efGetEyeFaceFlagParams, dll_handle, "efGetEyeFaceFlagParams");
    if (!fcnEfGetEyeFaceFlagParams)
        return -14;

    // link efGetLicenseExpirationDate()
    EF_LOAD_SHFCN(fcnEfGetLicenseExpirationDate, fcn_efGetLicenseExpirationDate, dll_handle, "efGetLicenseExpirationDate");
    if (!fcnEfGetLicenseExpirationDate)
        return -15;
    
#ifdef HASP_SECURE
    // link efHaspGetCurrentLoginKeyId()
    EF_LOAD_SHFCN(fcnEfHaspGetCurrentLoginKeyId, fcn_efHaspGetCurrentLoginKeyId, dll_handle, "efHaspGetCurrentLoginKeyId");
    if (!fcnEfHaspGetCurrentLoginKeyId)
        return -16;

    // link efHaspGetKeyIdsFromInfo()
    EF_LOAD_SHFCN(fcnEfHaspGetKeyIdsFromInfo, fcn_efHaspGetKeyIdsFromInfo, dll_handle, "efHaspGetKeyIdsFromInfo");
    if (!fcnEfHaspGetKeyIdsFromInfo)
        return -17;

    // link efHaspGetKeyInfo() 
    EF_LOAD_SHFCN(fcnEfHaspGetKeyInfo, fcn_efHaspGetKeyInfo, dll_handle, "efHaspGetKeyInfo");
    if (!fcnEfHaspGetKeyInfo)
        return -18;

    // link efHaspGetSessionKeyInfo()
    EF_LOAD_SHFCN(fcnEfHaspGetSessionKeyInfo, fcn_efHaspGetSessionKeyInfo, dll_handle, "efHaspGetSessionKeyInfo");
    if (!fcnEfHaspGetSessionKeyInfo)
        return -19;

    // link efHaspWriteC2VtoFile()
    EF_LOAD_SHFCN(fcnEfHaspWriteC2VtoFile, fcn_efHaspWriteC2VtoFile, dll_handle, "efHaspWriteC2VtoFile");
    if (!fcnEfHaspWriteC2VtoFile)
        return -20;

    // link efHaspWriteAllC2VtoOneFile()
    EF_LOAD_SHFCN(fcnEfHaspWriteAllC2VtoOneFile, fcn_efHaspWriteAllC2VtoOneFile, dll_handle, "efHaspWriteAllC2VtoOneFile");
    if (!fcnEfHaspWriteAllC2VtoOneFile)
        return -21;

    // link efHaspActivateV2C()
    EF_LOAD_SHFCN(fcnEfHaspActivateV2C, fcn_efHaspActivateV2C, dll_handle, "efHaspActivateV2C");
    if (!fcnEfHaspActivateV2C)
        return -22;
#endif
        
    return 0;
}

void freeDll()
{
    EF_FREE_LIB(dll_handle);
}

static double myGetTime()
{
   struct timespec ts;
   if (!clock_gettime(CLOCK_REALTIME, &ts))       
      return (double)((1000*1000*1000) * ts.tv_sec + ts.tv_nsec) / (1000.*1000.*1000.);
   return -1.;
}

void GetFaces(IplImage * image, EfVisualOutput *visual_output,double fps,int & face_count)
{
    if (VERTICAL_FLIP)
        cvFlip(image,image,0);
    if(visual_output)
    {
       face_count = 0;
        for (int i = 0; i < visual_output->num_vis; i++)
        {
             EfVisualData * vis = &(visual_output->vis_data[i]);
              if (!((vis->gender_confidence != 0.0 || vis->age != -1)&&(std::abs(vis->gender_confidence) > 0.2))) 
                continue;
            int tl_c, tl_r, tr_c, tr_r, bl_c, bl_r, br_c, br_r;
            tl_r = vis->current_position.top_left_row;
            tl_c = vis->current_position.top_left_col;
            tr_r = vis->current_position.top_right_row;
            tr_c = vis->current_position.top_right_col;
            bl_r = vis->current_position.bot_left_row;
            bl_c = vis->current_position.bot_left_col;
            br_r = vis->current_position.bot_right_row;
            br_c = vis->current_position.bot_right_col;
            int R = 220, G = 220, B = 220;
            int Rf = 255, Gf = 0,   Bf = 255; 
            int Rm = 0,   Gm = 0,   Bm = 255; 
            double angle = vis->angles[2];
            angle = std::min(angle, 90.0);
            angle = std::max(angle, -90.0);
            double width = sqrt((double)((tl_r - tr_r)*(tl_r - tr_r) + (tl_c - tr_c)*(tl_c - tr_c)));
            double offset_l = 1.0/4.0 * angle / 90.0 * width - width/2.0;
            double offset_r = 1.0/4.0 * angle / 90.0 * width + width/2.0;
            double unit_vec_c = (tr_c-tl_c)/width, unit_vec_r = (tr_r - tl_r)/width;
            double center_c = (tl_c+br_c)/2.0, center_r = (tl_r+br_r)/2.0;

            CvPoint * curve_face[1];
            CvPoint face_det[6];
            face_det[0] = cvPoint(tl_c, tl_r);
            face_det[1] = cvPoint(tr_c, tr_r);
            face_det[2] = cvPoint(int(center_c + offset_r*unit_vec_c), int(center_r+offset_r*unit_vec_r));
            face_det[3] = cvPoint(br_c, br_r);
            face_det[4] = cvPoint(bl_c, bl_r);
            face_det[5] = cvPoint(int(center_c + offset_l*unit_vec_c), int(center_r + offset_l*unit_vec_r));
            CvRect myfacerect=cvRect(face_det[0].x,face_det[1].y,abs(face_det[1].x-face_det[0].x),abs(face_det[4].y-face_det[0].y));

            int new_x=myfacerect.x - myfacerect.width*0.8;
            if(new_x < 0)
                new_x = 0;
            int new_y=myfacerect.y - myfacerect.height*0.8;
            if(new_y < 0)
                new_y = 0;

            int nw = myfacerect.width + myfacerect.width*1.6;
            if(new_x + nw > image->width)
            {
                nw = image->width-new_x-1;
            }
            int nh = myfacerect.height + myfacerect.height*1.6;
            if(new_y + nh >image->height)
            {
                nh = image->height-new_y-1;
            }
            myfacerect=cvRect(new_x,new_y,nw,nh);

            if(all_faces[face_count]!=0)
            {
                cvReleaseImage(&all_faces[face_count]);
            }
            all_faces[face_count]=cvCreateImage(cvSize(myfacerect.width,myfacerect.height),image->depth,image->nChannels);
            printf("myfacerect width=%d myfacerect height=%d\n",myfacerect.width,myfacerect.height);
            printf("allface[i].width=%d allface[i].height==%d d1=%d d2=%d\n",all_faces[face_count]->width,all_faces[face_count]->height,image->depth,all_faces[face_count]->depth);
            cvSetImageROI(image,myfacerect);
            cvCopy(image,all_faces[i]);
            cvResetImageROI(image);
            face_count++;
        }
    }
}

void Enroll(int argc, _TCHAR* argv[],IplImage* &img0,char* jpgdata,int jpglen)
{
    double start_time = (double)myGetTime();
    int done = false;
    double time = 0.0, prev_time, fps;
    if (!raw_img)
    {
        raw_img = new unsigned char[img0->width*img0->height];
        if (!raw_img)
        {
                printf("Cannot allocate memory for images.\n");
                return;
        }
     }
    printf("w=%d h=%d\n",img0->width,img0->height);
    if (!grey_image)
        grey_image = cvCreateImage(cvSize(img0->width, img0->height), 8, 1);

    cvCvtColor(img0,grey_image,CV_BGR2GRAY);

    int cnt = 0;
    for (int i = 0; i < grey_image->height; i++)
        for (int j = 0; j < grey_image->width; j++)
            raw_img[cnt++] = ((unsigned char*)(grey_image->imageData + i*grey_image->widthStep))[j];
    EfBoundingBox bounding_box;
    bounding_box.top_left_col = 0;  
    bounding_box.top_left_row = 0;  
    bounding_box.bot_right_col = img0->width - 1;
    bounding_box.bot_right_row = img0->height - 1;
    printf("3.1 raw_img=%x\n",raw_img);
    if (fcnEfMain(raw_img,img0->width,img0->height,VERTICAL_FLIP,&bounding_box,eyeface_state,time-start_time))
    {
        printf("Running efMain() failed.\n");
        return;
    }
    printf("4\n");
    EfVisualOutput * visual_output = NULL;
    double min_stability = 0.5; 
    int visible_at_least = 1;   
    if (!(visual_output = fcnEfGetVisualOutput(eyeface_state, min_stability, visible_at_least))) 
    {
        printf("Running efGetVisualOutput() failed.\n");
        return;
    }   
    int face_count = 0;
    GetFaces(img0,visual_output, fps,face_count);
    printf("detect %d faces\n", face_count);
    for(int i=0;i<face_count;i++)
    {
        char* pq = new char[sizeof(SendParam)];
        SendParam* op = (SendParam*)pq;
        op->img = cvCloneImage(all_faces[i]);
        char fname[30] = {0};
        sprintf(fname,"./%d.bmp",iname++);
        cvSaveImage(fname,all_faces[i]);
        SendInfo(op);
    }
    fcnEfFreeVisualOutput(visual_output);
}

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL) {
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
    if(res != CURLE_OK) 
    {
        LOG4CXX_TRACE(logger,"curl_easy_perform() failed: "<<curl_easy_strerror(res));
        picsize = 0;
    }
    else 
    {
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


void  *GetUrlPic(void *arg)
{
    getPic(buffer,nsize);
    if(nsize > 0)
    {
        std::vector<char> data1(buffer, buffer + nsize);
        cv::Mat imgMat = cv::imdecode(Mat(data1), CV_LOAD_IMAGE_COLOR);
        IplImage img = imgMat;
        printf("before enroll\n");
        IplImage* qq = &img;
        Enroll(0,0,qq,buffer,nsize);
        printf("end enroll\n");
        data1.clear();
        std::vector<char>().swap(data1);
    }
    printf("workmode %d\n",atoi(workmode));
    int sleepm = atoi(workmode);
    if(sleepm<0)
        sleepm = 50;
    usleep(1000*sleepm);
    return 0;
}



int main( int argc, char** argv )
{
    printf("\nExplicit linking ... ");
    int linking_error = 0;
    int file_not_found = 0;
    FILE * file = fopen(lib_name, "r");
    file_not_found = -100*(file == NULL);
    if (file)
        fclose(file);
    if (file_not_found || (linking_error = loadDll(lib_name)) != 0)
    {
        printf("failed. ERROR %d\n", linking_error + file_not_found);
        if (linking_error < -1)
            freeDll();
        return -1;
    }
    printf("done.\n");
#ifdef HASP_SECURE
    long long key = fcnEfHaspGetCurrentLoginKeyId();
#if defined(WIN32) || defined(_WIN32) || defined(_DLL) || defined(_WINDOWS_) || defined(_WINDLL)
    printf("Default key number: %I64d\n",key);
#else   
    printf("Default key number: %lld\n",key);
#endif
    EfHaspTime exp_time;
    int first_failed = 0;
    int second_failed = 0;        
    if (fcnEfGetLicenseExpirationDate(&exp_time, EYEDEA_PROD_EYEFACE_EXPERT)!=0)
    {
        printf("\nEyeFace Expert SDK license not available.\n");
        second_failed = 1;
    }
    else
        printf("\nEyeFace Expert SDK license expiration date [YYYY/MM/DD]: %d / %d / %d\n",exp_time.year,exp_time.month,exp_time.day);
    if (fcnEfGetLicenseExpirationDate(&exp_time, EYEDEA_PROD_EYEFACE_STANDARD)!=0)
    {
        printf("EyeFace Standard SDK license not available.\n");
        first_failed = 1;
    }
    else
        printf("EyeFace Light SDK license expiration date [YYYY/MM/DD]: %d / %d / %d\n",exp_time.year,exp_time.month,exp_time.day);

    if (first_failed && second_failed)
    {
        fprintf(stderr,"ERROR -101: HASP license verification failed.\n");
        freeDll();
        return 1;
    }
#endif
    printf("\nThe first EyeFace init ... ");
    if (!(eyeface_state = fcnEfInitEyeFace(MODULE_DIR, LOG_DIR, INI_DIR, INI_FILENAME)))
    {
        fprintf(stderr,"ERROR -103: efInitEyeFace() failed.\n");
        freeDll();
        return 1;
    }
    printf("done.\n");
    printf("1\n");
    PropertyConfigurator::configure("./ta_faceuploader_logconfig.cfg"); 
    logger = Logger::getLogger("Trace_FaceUpLoader"); 
    printf("2\n");
    pthread_mutex_init(&file_mutex,NULL); 
    strcpy(place,"japan");
    strcpy(pos,"Yodobashi Sapporo");
    strcpy(camname,"AXIS P3367-V");
    strcpy(mjpeg,"http://root:agent@192.168.0.101/jpg/image.jpg");
    strcpy(qurl,"amqp:tcp:219.101.248.183:9999");
    strcpy(qaddress,"message_queue; {create: always}");
    strcpy(workmode,"500");
    pthread_t  tid,tidsnd;
    int  ret;
    while(1)
    {
        GetUrlPic(0);
        SendMessageFormFile(0);
    }
    pthread_mutex_destroy(&file_mutex);
    return 0;
}
