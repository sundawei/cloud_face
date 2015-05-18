#include <stdio.h>
#include <vector>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/typeof/typeof.hpp> 
#include <boost/foreach.hpp>
#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Session.h>
#include <qpid/types/Variant.h>
#include <cstdlib>
#include <sstream>
#include <my_global.h>
#include <mysql.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>  
#include <unistd.h>  
#include <sys/stat.h>  
#include <ctype.h>
#include <iostream>
#include <sys/types.h>
#include <dirent.h>
#include <log4cxx/logger.h>    
#include <log4cxx/logstring.h> 
#include <log4cxx/propertyconfigurator.h> 
#include "EyeFace.h"
#include "EyeFaceAdvanced.h"






using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;
using std::stringstream;
using std::string;
using namespace log4cxx; 
using namespace boost::property_tree;

/*
#define SAFE_CALL(call, callname)                       \
        res = call;                                     \
        if(res != AYNX_OK)                              \
        {                                               \
                printf(callname "() error: %d\n", res); \
                LOG4CXX_TRACE(logger," "<<callname <<"error "<<res ); \
                return res;                             \
        }
//*/
const char *ageStrings[] = {"0-10", "5-15", "10-20", "15_25", "20-30", "25_35", "30-40", "35-45", "40-50", "45+"};
const char *MaleFemale[] = {"male", "female"};


char sql_server[30] = {0};
char sql_user[30] = {0};
char sql_passwd[30] = {0};
char sql_db[30] = {0};
char qpid_server[256] = {0};
char qpid_address[256] = {0};

//read settings
struct R_Setting
{
	string sql_server;
	string sql_user;
	string sql_passwd;
	string sql_db;
	string qpid_server;
	string qpid_address;
	string days;//diff days before
	string fv;//rec threshold value
};

float FV=0.0f;

int n=0;
//deal with interference data
int has_bdata=0;
void** bdata_afids = 0;
int bdata_counts = 0;

int DAYS=0;//diff days before

//set later incoming person's gender age 2 the old one
char sprvgender[20]={0};
char sprvage[20]={0};

#define _TCHAR char
#define _tprintf printf
#define _tfopen fopen
#define _fgetts fgets
#define _tcsstr strstr
#define _T(x) x

#ifndef _MAX_PATH
#define _MAX_PATH       1024
#endif

LoggerPtr logger=0;


void *engine =0;
// EyeFace-SDK shared library selection
// windows
//#if defined(WIN32) || defined(_WIN32) || defined(_DLL) || defined(_WINDOWS_) || defined(_WINDLL)
//   char lib_name[] = "..\\..\\eyefacesdk\\lib\\EyeFace.dll";
//#else //unix
 //  #if defined(x86_32) || defined(__i386__)
         char lib_name[] = "../../eyefacesdk/lib/libeyefacesdk-x86_64.so";
//   #else
//         char lib_name[] = "/opt/Release-EyeFaceSDK-v3.11.0-CentOS-6.4-x86_64-hasp/eyefacesdk/lib/libeyefacesdk-x86_64.so";
//   #endif
//#endif

//extern int  getGenderAge(char* img,int size,int &igender,int &iage);
//int  getGenderAge(char* img,int size,int &igender,int &iage);

typedef vector<R_Setting> SeT;
SeT readset( std::ifstream & is )//read server settings
{
	using boost::property_tree::ptree;
	ptree pt;
	read_xml(is, pt);

	SeT ans;
	BOOST_FOREACH( ptree::value_type const& v, pt.get_child("servers") ) 
	{
		if( v.first == "server" ) 
		{
			R_Setting f;
			f.sql_server = v.second.get<std::string>("sql_server");
			f.sql_user = v.second.get<std::string>("sql_user");
			f.sql_passwd = v.second.get<std::string>("sql_passwd");
			f.sql_db = v.second.get<std::string>("sql_db");
			f.qpid_server = v.second.get<std::string>("qpid_server");
			f.qpid_address = v.second.get<std::string>("qpid_address");
			f.days = v.second.get<std::string>("days");
			f.fv = v.second.get<std::string>("fv");
			ans.push_back(f);
		}
	}
	return ans;
}


// min function
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

// path to a directory containing config.ini file and models
#define EYEFACE_DIR   	(char*)"/opt/Release-EyeFaceSDK-v3.11.0-CentOS-6.4-x86_64-hasp/eyefacesdk"
#define CONFIG_INI 	    (char*)"config.ini"

// List of testing images
#define VERTICAL_FLIP 0
#define NUM_IMG 150
#ifndef _MAX_PATH
#define _MAX_PATH 1024
#endif

////////////////////////////////////////////////////////////////////
//                                                                //
//  Explicit linking of library functions                         //
//                                                                //
////////////////////////////////////////////////////////////////////
ef_shlib_hnd dll_handle;                                                  // global pointer to EyeFace-SDK shared library

// Declaration of pointers to library functions
fcn_efReadImageUc                    fcnEfReadImageUc;
fcn_efFreeImageUc                    fcnEfFreeImageUc;
fcn_efInitEyeFace                    fcnEfInitEyeFace;
fcn_efShutdownEyeFace                fcnEfShutdownEyeFace;
fcn_efFreeEyeFaceState               fcnEfFreeEyeFaceState;
fcn_efClearEyeFaceState              fcnEfClearEyeFaceState;
fcn_efRunFaceDetector                fcnEfRunFaceDetector;
fcn_efRunFaceLandmark                fcnEfRunFaceLandmark;
fcn_efFreeLandmarkResult             fcnEfFreeLandmarkResult;
fcn_efRunFaceImageExtraction         fcnEfRunFaceImageExtraction;
fcn_efRunFaceIDFeatureExtraction     fcnEfRunFaceIDFeatureExtraction;
fcn_efRunFaceGender                  fcnEfRunFaceGender;
fcn_efFreeGenderResult               fcnEfFreeGenderResult;
fcn_efRunFaceAge                     fcnEfRunFaceAge;
fcn_efFreeAgeResult                  fcnEfFreeAgeResult;
fcn_efRunFaceGenderAge               fcnEfRunFaceGenderAge;
fcn_efFreeGenderAgeResult            fcnEfFreeGenderAgeResult;
fcn_efFreeDetResult                  fcnEfFreeDetResult;
fcn_efUpdateTrackState               fcnEfUpdateTrackState;
fcn_efGetVisualOutput                fcnEfGetVisualOutput;
fcn_efFreeVisualOutput               fcnEfFreeVisualOutput;
fcn_efGetTrackFinalInfoOutput        fcnEfGetTrackFinalInfoOutput;
fcn_efFreeTrackFinalInfoOutput       fcnEfFreeTrackFinalInfoOutput;
fcn_efSetEyeFaceFlagParams           fcnEfSetEyeFaceFlagParams;
fcn_efGetEyeFaceFlagParams           fcnEfGetEyeFaceFlagParams;
fcn_efGetLicenseExpirationDate       fcnEfGetLicenseExpirationDate;
fcn_efIDCompareFeatureVectors        fcnEfIDCompareFeatureVectors;
    
// Explicit linking - needed for Sentinel LDK (aka HASP) software protection cyphered shared libraries
int loadDll(const char * dll_filename)
{
    EF_OPEN_SHLIB(dll_handle, dll_filename);
    if (!dll_handle)
    {
	printf("%s\n",dll_filename);
    	LOG4CXX_TRACE(logger,"Load dll failed ");
        return 1;
    }

    // link efReadImageUC() 
    EF_LOAD_SHFCN(fcnEfReadImageUc, fcn_efReadImageUc, dll_handle, "efReadImageUc");
    if (!fcnEfReadImageUc)
        return 2;

    // link efFreeImageUC()
    EF_LOAD_SHFCN(fcnEfFreeImageUc, fcn_efFreeImageUc, dll_handle,  "efFreeImageUc");
    if (!fcnEfFreeImageUc)
        return 3;

    // link efInitEyeFace()
    EF_LOAD_SHFCN(fcnEfInitEyeFace, fcn_efInitEyeFace, dll_handle, "efInitEyeFace");
    if (!fcnEfInitEyeFace)
        return 4;

    // link efShutdownEyeFace()
    EF_LOAD_SHFCN(fcnEfShutdownEyeFace, fcn_efShutdownEyeFace, dll_handle, "efShutdownEyeFace");
    if (!fcnEfShutdownEyeFace)
        return 5;

    // link efFreeEyeFaceState()
    EF_LOAD_SHFCN(fcnEfFreeEyeFaceState, fcn_efFreeEyeFaceState, dll_handle, "efFreeEyeFaceState");
    if (!fcnEfFreeEyeFaceState)
        return 6;

    // link efClearEyeFaceState()
    EF_LOAD_SHFCN(fcnEfClearEyeFaceState, fcn_efClearEyeFaceState, dll_handle, "efClearEyeFaceState");
    if (!fcnEfClearEyeFaceState)
        return 7;
    
    // link efRunFaceDetector()
    EF_LOAD_SHFCN(fcnEfRunFaceDetector, fcn_efRunFaceDetector, dll_handle, "efRunFaceDetector");
    if (!fcnEfRunFaceDetector)
        return 8;

    // link efRunFaceLandmark()
    EF_LOAD_SHFCN(fcnEfRunFaceLandmark, fcn_efRunFaceLandmark, dll_handle, "efRunFaceLandmark");
    if (!fcnEfRunFaceLandmark)
        return 9;

    // link efFreeLandmarkResult()
    EF_LOAD_SHFCN(fcnEfFreeLandmarkResult, fcn_efFreeLandmarkResult, dll_handle, "efFreeLandmarkResult");
    if (!fcnEfFreeLandmarkResult)
        return 10;

    // link efRunFaceImageExtraction()
    EF_LOAD_SHFCN(fcnEfRunFaceImageExtraction, fcn_efRunFaceImageExtraction, dll_handle, "efRunFaceImageExtraction");
    if (!fcnEfRunFaceImageExtraction)
        return 11;

    // link efRunFaceIDFeatureExtraction()
    EF_LOAD_SHFCN(fcnEfRunFaceIDFeatureExtraction, fcn_efRunFaceIDFeatureExtraction, dll_handle, "efRunFaceIDFeatureExtraction");
    if (!fcnEfRunFaceIDFeatureExtraction)
        return 12;

    // link efRunFaceGender()
    EF_LOAD_SHFCN(fcnEfRunFaceGender, fcn_efRunFaceGender, dll_handle, "efRunFaceGender");
    if (!fcnEfRunFaceGender)
        return 13;

    // link efFreeGenderResult()
    EF_LOAD_SHFCN(fcnEfFreeGenderResult, fcn_efFreeGenderResult, dll_handle, "efFreeGenderResult");
    if (!fcnEfFreeGenderResult)
        return 14;

    // link efRunFaceAge()
    EF_LOAD_SHFCN(fcnEfRunFaceAge, fcn_efRunFaceAge, dll_handle, "efRunFaceAge");
    if (!fcnEfRunFaceAge)
        return 15;

    // link efFreeAgeResult()
    EF_LOAD_SHFCN(fcnEfFreeAgeResult, fcn_efFreeAgeResult, dll_handle, "efFreeAgeResult");
    if (!fcnEfFreeAgeResult)
        return 16;

    // link efRunFaceGenderAge()
    EF_LOAD_SHFCN(fcnEfRunFaceGenderAge, fcn_efRunFaceGenderAge, dll_handle, "efRunFaceGenderAge");
    if (!fcnEfRunFaceGenderAge)
        return 17;

    // link efFreeGenderAgeResult() 
    EF_LOAD_SHFCN(fcnEfFreeGenderAgeResult, fcn_efFreeGenderAgeResult, dll_handle, "efFreeGenderAgeResult");
    if (!fcnEfFreeGenderAgeResult)
        return 18;

    // link efFreeDetResult()
    EF_LOAD_SHFCN(fcnEfFreeDetResult, fcn_efFreeDetResult, dll_handle, "efFreeDetResult");
    if (!fcnEfFreeDetResult)
        return 19;

    // link efUpdateTrackState() 
    EF_LOAD_SHFCN(fcnEfUpdateTrackState, fcn_efUpdateTrackState, dll_handle, "efUpdateTrackState");
    if (!fcnEfUpdateTrackState)
        return 20;

    // link efGetVisualOutput()
    EF_LOAD_SHFCN(fcnEfGetVisualOutput, fcn_efGetVisualOutput, dll_handle, "efGetVisualOutput");
    if (!fcnEfGetVisualOutput)
        return 21;

    // link efFreeVisualOutput()
    EF_LOAD_SHFCN(fcnEfFreeVisualOutput, fcn_efFreeVisualOutput, dll_handle, "efFreeVisualOutput");
    if (!fcnEfFreeVisualOutput)
        return 22;

    // link efGetTrackFinalInfoOutput()
    EF_LOAD_SHFCN(fcnEfGetTrackFinalInfoOutput, fcn_efGetTrackFinalInfoOutput, dll_handle, "efGetTrackFinalInfoOutput");
    if (!fcnEfGetTrackFinalInfoOutput)
        return 23;

    // link efFreeTrackFinalInfoOutput()
    EF_LOAD_SHFCN(fcnEfFreeTrackFinalInfoOutput, fcn_efFreeTrackFinalInfoOutput, dll_handle, "efFreeTrackFinalInfoOutput");
    if (!fcnEfFreeTrackFinalInfoOutput)
        return 24;

    // link efSetEyeFaceFlagParams()
    EF_LOAD_SHFCN(fcnEfSetEyeFaceFlagParams, fcn_efSetEyeFaceFlagParams, dll_handle, "efSetEyeFaceFlagParams");
    if (!fcnEfSetEyeFaceFlagParams)
        return 25;

    // link efGetEyeFaceFlagParams()
    EF_LOAD_SHFCN(fcnEfGetEyeFaceFlagParams, fcn_efGetEyeFaceFlagParams, dll_handle, "efGetEyeFaceFlagParams");
    if (!fcnEfGetEyeFaceFlagParams)
        return 26;

    // link efGetLicenseExpirationDate()
    EF_LOAD_SHFCN(fcnEfGetLicenseExpirationDate, fcn_efGetLicenseExpirationDate, dll_handle, "efGetLicenseExpirationDate");
    if (!fcnEfGetLicenseExpirationDate)
        return 27;

    // link fcnEfIDCompareFeatureVectors()
    EF_LOAD_SHFCN(fcnEfIDCompareFeatureVectors, fcn_efIDCompareFeatureVectors, dll_handle, "efIDCompareFeatureVectors");
    if (!fcnEfIDCompareFeatureVectors)
        return 28;

    return 0;
}

void freeDll()
{
    EF_FREE_LIB(dll_handle);
}

void* InitializeEngine(_TCHAR* fileName)
{
	loadDll(fileName);
	return 0;
}

void FinalizeEngine(void* engine)
{
	freeDll();
}

string GetDateString()//get date dir
{
	time_t now;
	struct tm *tm_now;
	char    sdatetime[20];

	time(&now);
	tm_now = localtime(&now);
	strftime(sdatetime, 20, "./%Y-%m-%d", tm_now);
	return std::string(sdatetime);
}

int finish_with_error(MYSQL *con)//close mysql error
{
	LOG4CXX_TRACE(logger,"mysql error " << mysql_error(con));
	mysql_close(con);
	return 0;      
}

int SaveBinaryFile(const void* data, unsigned int size, _TCHAR* name)
{
	FILE* fp = _tfopen(name, _T("wb"));
	if (fp == 0) {
		LOG4CXX_TRACE(logger,"failed to create " << name);
		return -1;
	}
	if (fwrite(data, size, 1, fp) == 1) {
		fclose(fp);
		return 0;
	}
	else {
		fclose(fp);
		return -1;
	}
}

int ReadBinaryFile(_TCHAR* name, void** data, unsigned int* size)
{
	void* pData = 0;
	FILE* fp = _tfopen(name, _T("rb"));
	if (fp == 0)
		return -1;
	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	pData = malloc(*size * sizeof(unsigned char));
	if (data == 0) {
		LOG4CXX_TRACE(logger,"out of memory " << name);
		fclose(fp);
		return -1;
	}
	*data = pData;
	memset(pData, 0, *size);
	if (fread(pData, *size, 1, fp) != 1) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;
}
void getbdirs(long tttm,int days,vector<string>& salldirs)//find available date dirs 
{
        for(int i=0;i<days;i++)
        {
                long atm = tttm - i*24*3600;
                struct tm *tm_atm = localtime(&atm);
                char adir[16] = {0};
                strftime(adir,16,"./%Y-%m-%d",tm_atm);
                DIR   *dp;
                if((dp=opendir(adir))==NULL)
                {
                        LOG4CXX_TRACE(logger," "<<adir <<" not exists");
                }
                else
                {
                        LOG4CXX_TRACE(logger,"add a date dir "<<adir);
                        salldirs.push_back(adir);
                }
                closedir(dp);

        }
}
void getdirfiles(const char* sdir,unsigned int & count,vector<string>& fname)//get filelist from dir
{
	std::vector<string> vdirs;
	getbdirs(time(0),DAYS,vdirs);
	count = 0;
	if(vdirs.size()==0||strcmp(sdir,"./bdata")==0)//bddir spec
	{	

		DIR   *dp;   
		struct   dirent   *dirp;    
		if((dp=opendir(sdir))==NULL)   
		{   
			LOG4CXX_TRACE(logger,"no dir " << sdir);
			count = 0;
			return ;   
		}


		int ret = 0;

		while((dirp=readdir(dp))!=NULL)   
		{   

			if(strcmp(dirp->d_name,".")==0   ||   strcmp(dirp->d_name,"..")==0)   
				continue;
			string sall = string(sdir)+string("/")+string(dirp->d_name);

			fname.push_back(sall);
			LOG4CXX_TRACE(logger,"push file " << sall);

			ret++;   
		}	 
		count += ret;
		closedir(dp);
	}
	else
	{
		for(int i=0;i<static_cast<int>(vdirs.size());i++)
		{
			DIR   *dp;   
			struct   dirent   *dirp;    
			if((dp=opendir(vdirs.at(i).c_str()))==NULL)   
			{   
				LOG4CXX_TRACE(logger,"no dir " << sdir);
				count = 0;
				closedir(dp);
				continue ;   
			}


			int ret = 0;

			while((dirp=readdir(dp))!=NULL)   
			{	   

				if(strcmp(dirp->d_name,".")==0   ||   strcmp(dirp->d_name,"..")==0)   
					continue;
				string sall = vdirs.at(i)+string("/")+string(dirp->d_name);

				fname.push_back(sall);
				LOG4CXX_TRACE(logger,"push file " << sall);

				ret++;   
			}	 
			count += ret;
			closedir(dp);
		}
	}

}
void Readbdata()//read non face data
{
	vector<string> vbdatafilename;
	unsigned int count;
	getdirfiles("./bdata",count,vbdatafilename);
	if(count>0)
	{
		has_bdata = 1;
		bdata_counts = count;
		bdata_afids = (void **)malloc(count * sizeof(void *));
		memset(bdata_afids, 0, count * sizeof(void *));
		for (unsigned int i = 0; i < count; i++) {
			unsigned int afidSize = 0;
			void* pAfid = 0;
			char onefile[256] = {0};
			strcpy(onefile,vbdatafilename.at(i).c_str());
			int res = ReadBinaryFile(onefile, &pAfid, &afidSize);
			if (res != 0) {
				LOG4CXX_TRACE(logger,_T("failed to read ") << vbdatafilename.at(i).c_str());
				std::vector<string>().swap(vbdatafilename);
				return ;
			}
			bdata_afids[i] = pAfid;
		}
		LOG4CXX_TRACE(logger,"found bdata counts =  " << bdata_counts);
	}
}
string DealBadData(string facedata)//verify wether spec face is not a face
{
	//read bad data afid
	return "";
}
int GetFileFacID(EfDetResult * &det_result_1,char* filename)
{
	void * state = NULL;  
	EfMatrixUc * input_image_1 = NULL;

printf("05\n");
	 if (!(state = fcnEfInitEyeFace(EYEFACE_DIR,EYEFACE_DIR,EYEFACE_DIR,CONFIG_INI)))
 {
     fprintf(stderr,"ERROR 103: efInitEyeFace() failed.\n");
     //freeDll();
     return 103;
 }
 printf("06\n");
 printf("filename %s\n",filename);

	if (!(input_image_1 = fcnEfReadImageUc(filename)))
    {
        LOG4CXX_TRACE(logger,"ERROR 105: Can't load the image file");
      //  freeDll();
        return 105;
    }

    if (!(det_result_1 = fcnEfRunFaceDetector(input_image_1, state)))
    {
        LOG4CXX_TRACE(logger,"ERROR 106: efRunFaceDetector() failed.");
      //  freeDll();
        return 106;
    }
    if(det_result_1->num_detections<1)
    {
    	 fcnEfFreeImageUc(input_image_1);
    fcnEfShutdownEyeFace(state);
    fcnEfClearEyeFaceState(state);
    fcnEfFreeEyeFaceState(state);
    	return 170;//no face
    }
 printf("07\n");
    for (int i = 0;  i < det_result_1->num_detections; i++)
    {
            // Face Landmark part
        EfLandmarkResult * landmark_result = NULL;
        if (!(landmark_result = fcnEfRunFaceLandmark(input_image_1, &(det_result_1->detection[i]), state)))
        {
            //fprintf(stderr,"ERROR 107: efRunFaceLandmark() failed.\n");
            LOG4CXX_TRACE(logger,"ERROR 107: efRunFaceLandmark() failed.");
          //  freeDll();
            return 107;
        }

		printf("08\n");
        if (landmark_result->error == 1)
            //fprintf(stderr,"Warning: Can't estimate face landmarks, augmented face BB goes out of the image.\n");
        	LOG4CXX_TRACE(logger,"Warning: Can't estimate face landmarks, augmented face BB goes out of the image.");
        // Face Extraction part
        if (fcnEfRunFaceImageExtraction(input_image_1, &(det_result_1->detection[i]), state,
                    landmark_result))
        {
            //fprintf(stderr, "ERROR 108: Face extraction failed\n");
            LOG4CXX_TRACE(logger,"ERROR 108: Face extraction failed.");
           // freeDll();
            return 108;
        }

        // free landmark result //
        fcnEfFreeLandmarkResult(landmark_result);
    }
    printf("09\n");
    for (int i = 0; i < det_result_1->num_detections; i++)
    {
		int frontal_face = 0;
		if(det_result_1->detection[i].angles[2]>0)
			frontal_face = det_result_1->detection[i].angles[2];
		else
			frontal_face = 0 - det_result_1->detection[i].angles[2];
        frontal_face = frontal_face < 30; // only compute face attributes for frontal faces!!!
        if (frontal_face)
        {
            if (fcnEfRunFaceIDFeatureExtraction(&(det_result_1->detection[i]), state) == -1)
            {
                  fprintf(stderr,"ERROR 112: Face ID features computation failed.\n");
                  return 112;
            }
        }
    }
    printf("10\n");
    fcnEfFreeImageUc(input_image_1);
    fcnEfShutdownEyeFace(state);
    fcnEfClearEyeFaceState(state);
    fcnEfFreeEyeFaceState(state);
    return 1;

}

int GetFaceID(EfDetResult * &det_result_1,string facedata)
{
	void * state = NULL;  
	EfMatrixUc * input_image_1 = NULL;
	FILE* ffh = fopen("./temp1.jpg","wb");
	fwrite((char*)facedata.data(),facedata.size(),1,ffh);
	fclose(ffh);
	printf("01\n");
 if (!(state = fcnEfInitEyeFace(EYEFACE_DIR,EYEFACE_DIR,EYEFACE_DIR,CONFIG_INI)))
 {
     fprintf(stderr,"ERROR 103: efInitEyeFace() failed.\n");
     //freeDll();
     return 103;
 }
	printf("01.5\n");
	if (!(input_image_1 = fcnEfReadImageUc("./temp1.jpg")))
    {
        LOG4CXX_TRACE(logger,"ERROR 105: Can't load the image file");
       // freeDll();
        return 105;
    }
	printf("02\n");
    if (!(det_result_1 = fcnEfRunFaceDetector(input_image_1, state)))
    {
        LOG4CXX_TRACE(logger,"ERROR 106: efRunFaceDetector() failed.");
       // freeDll();
        return 106;
    }
	printf("03\n");
    if(det_result_1->num_detections<1)
    {
    	 fcnEfFreeImageUc(input_image_1);
    fcnEfShutdownEyeFace(state);
    fcnEfClearEyeFaceState(state);
    fcnEfFreeEyeFaceState(state);
    	return 170;//no face
    }
    for (int i = 0;  i < det_result_1->num_detections; i++)
    {
            // Face Landmark part
        EfLandmarkResult * landmark_result = NULL;
        if (!(landmark_result = fcnEfRunFaceLandmark(input_image_1, &(det_result_1->detection[i]), state)))
        {
            //fprintf(stderr,"ERROR 107: efRunFaceLandmark() failed.\n");
            LOG4CXX_TRACE(logger,"ERROR 107: efRunFaceLandmark() failed.");
          //  freeDll();
            return 107;
        }

        if (landmark_result->error == 1)
            //fprintf(stderr,"Warning: Can't estimate face landmarks, augmented face BB goes out of the image.\n");
        	LOG4CXX_TRACE(logger,"Warning: Can't estimate face landmarks, augmented face BB goes out of the image.");
        // Face Extraction part
        if (fcnEfRunFaceImageExtraction(input_image_1, &(det_result_1->detection[i]), state,
                    landmark_result))
        {
            //fprintf(stderr, "ERROR 108: Face extraction failed\n");
            LOG4CXX_TRACE(logger,"ERROR 108: Face extraction failed.");
          //  freeDll();
            return 108;
        }
	printf("04\n");
        // free landmark result //
        fcnEfFreeLandmarkResult(landmark_result);
    }
    for (int i = 0; i < det_result_1->num_detections; i++)
    {
		int frontal_face = 0;
		if(det_result_1->detection[i].angles[2]>0)
			frontal_face = det_result_1->detection[i].angles[2];
		else
			frontal_face = 0 - det_result_1->detection[i].angles[2];
        frontal_face = frontal_face < 30.0; // only compute face attributes for frontal faces!!!
        if (frontal_face)
        {
            if (fcnEfRunFaceIDFeatureExtraction(&(det_result_1->detection[i]), state) == -1)
            {
                  fprintf(stderr,"ERROR 112: Face ID features computation failed.\n");
                  return 112;
            }
        }
    }
    fcnEfFreeImageUc(input_image_1);
    fcnEfShutdownEyeFace(state);
    fcnEfClearEyeFaceState(state);
    fcnEfFreeEyeFaceState(state);
    return 1;

}

float CompareFace(EfDetResult* &e1,EfDetResult* &e2)
{
	void * state = NULL; 
	if (!(state = fcnEfInitEyeFace(EYEFACE_DIR,EYEFACE_DIR,EYEFACE_DIR,CONFIG_INI)))
 {
     fprintf(stderr,"ERROR 103: efInitEyeFace() failed.\n");
     //freeDll();
     return 103;
 }
	if(e1->num_detections>0&&e2->num_detections>0)
	{
		double response = 0.0;
		fcnEfIDCompareFeatureVectors(e1->detection[0].feat_id,e2->detection[0].feat_id,state,&response);
		fcnEfShutdownEyeFace(state);
		fcnEfClearEyeFaceState(state);
		fcnEfFreeEyeFaceState(state);
		return response;								
	}
	else
	{
		 fcnEfShutdownEyeFace(state);
		 fcnEfClearEyeFaceState(state);
		 fcnEfFreeEyeFaceState(state);
		return 0.0f;
	}
}
string FindMatchFace2(string facedata,string pkgdir)
{
	string rets="";
	EfDetResult *det_result_1;
	if(1!=GetFaceID(det_result_1,facedata))
	{
		fcnEfFreeDetResult(det_result_1);
		return "";
	}

	std::vector<string> v;
	unsigned int nAfids = 0;
	char sdir[20]={0};
	//strcpy(sdir,GetDateString().c_str());
	strcpy(sdir,pkgdir.c_str());
	getdirfiles(sdir,nAfids,v);

	for(unsigned int i = 0; i < nAfids; i++)
	{
		char onefile[256] = {0};
		strcpy(onefile,v.at(i).c_str());
		EfDetResult *det_result_2;
		if(1!=GetFileFacID(det_result_2,onefile))
		{
			fcnEfFreeDetResult(det_result_2);
			continue;
		}	
		printf("11\n");
		float rate=CompareFace(det_result_1,det_result_2);
		printf("12\n");
		if(rate>3.0f)
		{
			rets = v.at(i);
			fcnEfFreeDetResult(det_result_2);
			break;
		}
	}
	fcnEfFreeDetResult(det_result_1);
	return rets;

}
/*
string FindMatchFace(string facedata,string pkgdir)
{
	string rets="";
	AynxImage *img = 0;

	int  res = AFIDDecodeImage((char*)facedata.data(),facedata.size(),&img);
	if (res != AYNX_OK) {
		LOG4CXX_TRACE(logger,"failed to load image file " << res);
		return std::string("");
	}

	std::vector<string> v;
	unsigned int nAfids = 0;
	char sdir[20]={0};
	//strcpy(sdir,GetDateString().c_str());
	strcpy(sdir,pkgdir.c_str());
	getdirfiles(sdir,nAfids,v);

	// allocate and load afids

	void** afids = (void **)malloc(nAfids * sizeof(void *));
	if (afids == 0) {
		LOG4CXX_TRACE(logger,_T("failed to allocate afids array"));
		return std::string("");
	}
	memset(afids, 0, nAfids * sizeof(void *));
	for (unsigned int i = 0; i < nAfids; i++) {
		unsigned int afidSize = 0;
		void* pAfid = 0;
		char onefile[256] = {0};
		strcpy(onefile,v.at(i).c_str());
		res = ReadBinaryFile(onefile, &pAfid, &afidSize);
		if (res != AYNX_OK) {
			LOG4CXX_TRACE(logger,_T("failed to read ") << v.at(i).c_str());
			return std::string("");
		}
		afids[i] = pAfid;
	}

	AynxFaces* faces = 0;

	// detect and preprocess faces in the image
	res = AFIDDetectFaces(engine, img, &faces);
	if (res != AYNX_OK) {
		LOG4CXX_TRACE(logger,"failed to detect faces " << res);
		return std::string("");
	}

	if (faces->nFaces > 0) {
		res = AFIDPreprocessFaces(engine, faces);
		if (res != AYNX_OK) {
			LOG4CXX_TRACE(logger,"failed to preprocess faces " << res);
			return std::string("");
		}

		// identify faces
		res = AFIDIdentifyFaces(engine, faces, afids, nAfids);
		if (res != AYNX_OK) {
			LOG4CXX_TRACE(logger,"failed to identify faces  " << res);
			return std::string("");
		}

		for (unsigned int i = 0; i < faces->nFaces; i++) {

			LOG4CXX_TRACE(logger,"face  " << i+1);
			LOG4CXX_TRACE(logger,faces->faces[i].location.x <<","<<faces->faces[i].location.y <<","<<faces->faces[i].location.w <<","<<faces->faces[i].location.h );

			if (!(faces->faces[i].id < 0))
			{
				LOG4CXX_TRACE(logger," " << v.at(faces->faces[i].id).c_str() <<" "<<100.0f * faces->faces[i].score <<"%");
				if(faces->faces[i].score>FV)
				//rets = v.at(faces->faces[i].id).substr(13,36);//updated afid dir is not correct,here add path
					rets = v.at(faces->faces[i].id);
				else
				rets = "";
			}
			else
			{
				LOG4CXX_TRACE(logger,"unknown");
				rets = "";
			}
		}
	}
	else
		LOG4CXX_TRACE(logger,"no faces");

	if(img!=0)
		AFIDReleaseImage(img);

	AFIDReleaseFaces(faces);

	// release resources
	for (unsigned int i = 0; i < nAfids; i++)
		free(afids[i]);
	free(afids);

	//std::vector<string>(v).swap(v);
	v.clear();
	std::vector<string>().swap(v);
	LOG4CXX_TRACE(logger,"return rest="<<rets);
	return rets;

}
//*/

int SendFace2DB(int len0,char* facedata,int len1,char* afid,const char* place,const char* pos,int workmode,const char* camname,const char* suuid,const char* stime,const char* sgender,const char* sage,int whichdb=0)
//whichdb 0:face 1:faceall
{


	int size = len0;
	int size2 = len1;

	MYSQL *con = mysql_init(NULL);

	if (con == NULL)
	{
		LOG4CXX_TRACE(logger,"mysql_init() failed");
		return 0;
	}  

	if (mysql_real_connect(con, sql_server, sql_user, sql_passwd, 
		sql_db, 0, NULL, 0) == NULL) 
	{
		return finish_with_error(con);
	}   

	char *chunk=new char[2*size+1];
	char *chunk2=new char[2*size2+1];

	mysql_real_escape_string(con, chunk, facedata, size);
	mysql_real_escape_string(con, chunk2, afid, size2);

	const char *st = "INSERT INTO face(face, afid, dt, afidlen, place, pos, workmode, camname, uuid, gender, age) VALUES('%s', '%s',FROM_UNIXTIME(%d),'%d','%s','%s','%d','%s','%s','%s','%s')";
	const char *st1 = "INSERT INTO faceall(face, afid, dt, afidlen, place, pos, workmode, camname, uuid, gender, age) VALUES('%s', '%s',FROM_UNIXTIME(%d),'%d','%s','%s','%d','%s','%s','%s','%s')";
	size_t st_len;
	if(whichdb == 0)
	{
		st_len = strlen(st);
	}
	else
	{
		st_len = strlen(st1);
	}

	char *query=new char[st_len + 2*size+1 + 100+40+80 +2*size2+1]; 
	int len=0;
	if(whichdb==0)
		len = snprintf(query, st_len + 2*size+1+100+40+80+2*size2+1, st, chunk,chunk2,atoi(stime),size2,place,pos,workmode,camname,suuid,sgender,sage);
	else
		len = snprintf(query, st_len + 2*size+1+100+40+80+2*size2+1, st1, chunk,chunk2,atoi(stime),size2,place,pos,workmode,camname,suuid,sgender,sage);

	if (mysql_real_query(con, query, len))
	{
		delete [] chunk;
		delete [] chunk2;
		delete [] query;
		return finish_with_error(con);
	}

	mysql_close(con);
	delete [] chunk;
	delete [] chunk2;
	delete [] query;
	return 1;
}

void UpdateDBAfid(string suuid,string safiddata,int ntime)//change afid of a face
{
	MYSQL *con = mysql_init(NULL);

	int sfaidlen = suuid.size();

	string cs = suuid.substr(sfaidlen-36,36);

	if (con == NULL)
	{
		LOG4CXX_TRACE(logger,"mysql_init() failed");
		return;
	}  

	if (mysql_real_connect(con, sql_server, sql_user, sql_passwd, 
		sql_db, 0, NULL, 0) == NULL) 
	{
		finish_with_error(con);
		return;
	} 

	char *chunk1 = new char[2*safiddata.size()+1];
	mysql_real_escape_string(con, chunk1, safiddata.data(), safiddata.size());

	const char* qs="UPDATE face SET afid='%s', ldt=FROM_UNIXTIME(%d) WHERE uuid='%s'";
	size_t st_len = strlen(qs);
	char *query=new char[st_len + 2*safiddata.size()+1+80];

	int len= snprintf(query,st_len + 2*safiddata.size()+1+80,qs,chunk1,ntime,cs.c_str());


	if (mysql_real_query(con, query, len))
	{
		delete [] chunk1;
		delete [] query;
		finish_with_error(con);
		return;
	} 

	mysql_close(con);
	delete [] chunk1;
	delete [] query;

}


void QuerySameAfidFaces(vector<string>& allfaces,string suuid)
{
	MYSQL *con = mysql_init(NULL);

	int sfaidlen = suuid.size();

	string cs = suuid.substr(sfaidlen-36,36);

	if (con == NULL)
	{
		LOG4CXX_TRACE(logger,"mysql_init() failed");
		return;
	}  

	if (mysql_real_connect(con, sql_server, sql_user, sql_passwd, 
		sql_db, 0, NULL, 0) == NULL) 
	{
		finish_with_error(con);
		return;
	} 
	string querys = "SELECT Id,face FROM faceall WHERE uuid ='"+cs+"'";

	if(mysql_query(con, querys.c_str()))
	{
		finish_with_error(con);
		return;
	}

	MYSQL_RES *result = mysql_store_result(con);

	if (result == NULL) 
	{
		finish_with_error(con);
		return;
	}  

	int num_fields = mysql_num_fields(result);

	MYSQL_ROW row;
	int tc=0;
	while ((row = mysql_fetch_row(result)))
	{
		unsigned long *lengths;
		lengths = mysql_fetch_lengths(result);
		if(lengths==NULL)
		{
			break;
		}

		for(int i = 0; i < num_fields; i++)
		{
			if(i==1)
			{
				string sp;
				sp.assign(row[i],lengths[i]);
				allfaces.push_back(sp);
			}
		}
		tc++;
		if(tc>=30)
			break;
	}
	mysql_free_result(result);
	mysql_close(con);
}


void UpdateAfid(vector<string>& allfaces,string suuid,int newfacetime,string pkgdir)
{
	//升级 日期目录下的afid文件,重新用多张人脸数据enroll
	/*
	unsigned int nImages = allfaces.size();
	int res = 0;

	// allocate faces storage
	AynxFaces** faces = (AynxFaces **)malloc(nImages * sizeof(AynxFaces *));
	if (faces == 0) {
		LOG4CXX_TRACE(logger,"failed to allocate faces");
		return;
	}
	memset(faces, 0, nImages * sizeof(AynxFaces *));

	// allocate faces collection for enrollment
	AynxFacesCollection facesCollection;
	facesCollection.nFaces = 0;
	facesCollection.faces = (AynxFaces *)malloc(nImages * sizeof(AynxFaces));
	if (facesCollection.faces == 0) {
		LOG4CXX_TRACE(logger,"failed to allocate faces collection");
		return;
	}
	memset(facesCollection.faces, 0, nImages * sizeof(AynxFaces));

	// detect and preprocess faces in the images and put them to faces collection for enrollment
	for (unsigned int i = 0; i < nImages; i++) {

		AynxImage *img = 0;
		//res = AFIDLoadImageA(imagePath, &img);
		LOG4CXX_TRACE(logger,"begin decode "<<allfaces.at(i).size());
		res = AFIDDecodeImage((char*)allfaces.at(i).data(),allfaces.at(i).size(),&img);
		LOG4CXX_TRACE(logger,"end decode ");
		if (res == AYNX_OK) {


			// detect faces
			res = AFIDDetectFaces(engine, img, &faces[i]);

			if (res != AYNX_OK) {
				LOG4CXX_TRACE(logger,"failed to detect faces "<< res);
				return ;
			}

			// put detected faces to faces collection for enrollment
			memcpy(facesCollection.faces + i, faces[i], sizeof(AynxFaces));
			facesCollection.nFaces++;

			if (faces[i]->nFaces == 0) {
				LOG4CXX_TRACE(logger,"no faces");
				AFIDReleaseImage(img);
				continue;
			}

			// preprocess faces
			res = AFIDPreprocessFaces(engine, faces[i]);
			if (res != AYNX_OK) {
				LOG4CXX_TRACE(logger,"failed to preprocess faces" << res);
				return;
			}


			LOG4CXX_TRACE(logger," " << faces[i]->faces[0].location.x<<" "<<faces[i]->faces[0].location.y<<" "<<faces[i]->faces[0].location.w<<" "<<faces[i]->faces[0].location.h);
		}
		else {

			LOG4CXX_TRACE(logger,"failed to read " <<res);
			return;
		}
		if(img!=0)
			AFIDReleaseImage(img);
	}

	void* afid = 0;
	unsigned int size = 0;
	// enroll from faces collection
	LOG4CXX_TRACE(logger,"update enrolling... 3");

	string safiddata;

	res = AFIDEnrollPerson(engine, &facesCollection, &afid, &size);
	if (res == AYNX_OK) {
		LOG4CXX_TRACE(logger,"ok");
		//mkdir(GetDateString().c_str(),0775);
		mkdir(pkgdir.c_str(),0775);
		char fn[100]={0};
		memset(fn,0,100);
		//strcpy(fn,(GetDateString()+string("/")+suuid).c_str());
		strcpy(fn,(pkgdir+string("/")+suuid).c_str());
		res = SaveBinaryFile(afid,size,fn);
		safiddata.assign((char*)afid,size);
		if (res != AYNX_OK)
		{
			LOG4CXX_TRACE(logger,"failed to save afid");
		}
		LOG4CXX_TRACE(logger,"success enrolling.... 2");
		AFIDReleaseAfid(afid);
	}
	else
	{
		AFIDReleaseAfid(afid);
		LOG4CXX_TRACE(logger,"failed "<<res<<" face count "<<facesCollection.nFaces);
	}

	// free resources
	for (unsigned int i = 0; i < nImages; i++)
		AFIDReleaseFaces(faces[i]);
	free(faces);
	free(facesCollection.faces);

	//升级 数据库中safid的afid和最后入库时间
	if(safiddata.size()>0)
	//*/
	string safiddata = "";
	UpdateDBAfid(suuid,safiddata,newfacetime);
}


void getga(int& g,int& a)
{
	FILE *fp=popen("./getGenderAge2","r");
	if(fp==NULL)
		return ;
	char buf[10]={0};
	
	int count=0;
	LOG4CXX_TRACE(logger,"before getga");
	while(fgets(buf,10,fp)!=NULL)
	{
		//printf("%d,%d\n",++count,atoi(buf));
		count++;
		if(count==1)
			g=atoi(buf);
		if(count==2)
		{
			a=atoi(buf);
		}

	}
	LOG4CXX_TRACE(logger,"end getga");

	if(pclose(fp)==-1)
	{
		//printf("error pclose\n");
		LOG4CXX_TRACE(logger,"error pclose "<<errno);
	}
}


void getdbgenderage(string suuid)//search db 2 find first rec gender and age
{
	MYSQL *con = mysql_init(NULL);

	//int sfaidlen = suuid.size();

	string retstr="";

	string cs = suuid;

	if (con == NULL)
	{
		LOG4CXX_TRACE(logger,"mysql_init() failed");
		return;
	}  

	if (mysql_real_connect(con, sql_server, sql_user, sql_passwd, 
		sql_db, 0, NULL, 0) == NULL) 
	{
		finish_with_error(con);
		return;
	} 
	string querys = "SELECT Id,gender,age FROM face WHERE uuid ='"+cs+"'";

	if(mysql_query(con, querys.c_str()))
	{
		finish_with_error(con);
		return;
	}

	MYSQL_RES *result = mysql_store_result(con);

	if (result == NULL) 
	{
		finish_with_error(con);
		return;
	}  

	int num_fields = mysql_num_fields(result);

	MYSQL_ROW row;
	int tc=0;
	while ((row = mysql_fetch_row(result)))
	{
		unsigned long *lengths;
		lengths = mysql_fetch_lengths(result);
		if(lengths==NULL)
		{
			break;
		}

		for(int i = 0; i < num_fields; i++)
		{
			if(i==1)
			{
				//string sp;
				//sp.assign(row[i],lengths[i]);
				//allfaces.push_back(sp);
				//sgender.assign(row[i],lengths[i]);
				memcpy(sprvgender,row[i],lengths[i]);
			}
			if(i==2)
			{
				//age.assign(row[i],lengths[i]);
				memcpy(sprvage,row[i],lengths[i]);
			}
		}
		tc++;
		break;
	}
	mysql_free_result(result);
	mysql_close(con);
}

int main2(int argc, char** argv) {
	const char* url = argc>1 ? argv[1] : qpid_server;
	const char* address = argc>2 ? argv[2] : qpid_address;
	std::string connectionOptions = argc > 3 ? argv[3] : "";

	Connection connection(url, connectionOptions);
	connection.setOption("reconnect", true);
	try {
		connection.open();
		Session session = connection.createSession();
		Receiver receiver = session.createReceiver(address);
		Variant::Map content;
		decode(receiver.fetch(Duration::SECOND*60), content);
		string s=content["picture"];
		string splace=content["place"];
		string spos = content["pos"];
		string sworkmode=content["workmode"];
		string scamname=content["camname"];
		string suuid = content["uuid"];
		string ssti = content["int"];

		time_t now=atoi(ssti.c_str());
		struct tm *tm_now;
		char    sdatetime[50]={0};
		char pkg_date_dir[16]={0};
		//time(&now);
		tm_now = localtime(&now);
		strftime(sdatetime, 50, "PKG time %Y-%m-%d %H:%M:%S", tm_now);
		strftime(pkg_date_dir,16,"./%Y-%m-%d",tm_now);
		string pkg_dir=pkg_date_dir;
		printf("%s\n",pkg_dir.c_str());
		LOG4CXX_TRACE(logger,pkg_dir.c_str());
		LOG4CXX_TRACE(logger," " <<tm_now<<" "<<atoi(ssti.c_str())<<" "<<sdatetime);




		//unsigned int nImages = 1;
		// allocate faces storage
		// AynxFaces** faces = (AynxFaces **)malloc(nImages * sizeof(AynxFaces *));
		// if (faces == 0) {
		// 	LOG4CXX_TRACE(logger,"failed to allocate faces rv2db exit");
		// 	exit(1);
		// }
		// memset(faces, 0, nImages * sizeof(AynxFaces *));

		// // allocate faces collection for enrollment
		// AynxFacesCollection facesCollection;
		// facesCollection.nFaces = 0;
		// facesCollection.faces = (AynxFaces *)malloc(nImages * sizeof(AynxFaces));
		// if (facesCollection.faces == 0) {
		// 	//_tprintf(_T("failed to allocate faces collection\n"));
		// 	LOG4CXX_TRACE(logger,"failed to allocate faces collection rv2db exit");
		// 	exit(1);
		// }
		// memset(facesCollection.faces, 0, nImages * sizeof(AynxFaces));

		// AynxImage *img = 0;
		// int	res = AFIDDecodeImage((char*)s.data(),s.size(),&img);

//		while (res == AYNX_OK) {
		while(1) {
			// detect faces
			// res = AFIDDetectFaces(engine, img, &faces[0]);
			// //  res =  AFIDDetectFacesInImage(engine,img,&faces[i]);
			// if (res != AYNX_OK) {
			// 	//_tprintf(_T("failed to detect faces %d\n"), res);
			// 	LOG4CXX_TRACE(logger,"failed to allocate faces collection rv2db exit");
			// 	break;
			// }

			// // put detected faces to faces collection for enrollment
			// memcpy(facesCollection.faces + 0, faces[0], sizeof(AynxFaces));
			// facesCollection.nFaces++;

			// if (faces[0]->nFaces == 0) {
			// 	AFIDReleaseImage(img);
			// 	img=0;
			// 	LOG4CXX_TRACE(logger,"detect 0 faces");
			// 	break;
			// }
			//
			printf("got here\n");
			EfDetResult * det_result_1;
			
			if(1!=GetFaceID(det_result_1,s))
			{
				break;
			}
			fcnEfFreeDetResult(det_result_1);


			// res = AFIDPreprocessFaces(engine, faces[0]);
			// if (res != AYNX_OK) {
			// 	LOG4CXX_TRACE(logger,"failed to preprocess faces "<<res);
			// 	break;
			// }
			// else
			{
				void* afid = 0;
				unsigned int size = 0;
				// // enroll from faces collection
				// LOG4CXX_TRACE(logger,"enrolling... 1");

				// LOG4CXX_TRACE(logger,"befor AFIDEnrollPerson " << facesCollection.nFaces <<" "<<engine);
				// res = AFIDEnrollPerson(engine, &facesCollection, &afid, &size);
				// LOG4CXX_TRACE(logger,"after AFIDEnrollPerson "<<facesCollection.nFaces);
				// if (res == AYNX_OK) {
				if(1) {
					LOG4CXX_TRACE(logger,"ok");
					//res = SaveBinaryFile(afid, size, argv[1]);
					// if(DealBadData(s)!="")//bdata waste time to deal
					// {
					// 	LOG4CXX_TRACE(logger,"*******BDDATA********* wasting time");
					// 	AFIDReleaseAfid(afid);
					// 	break;
					// }

					string sfuuid = FindMatchFace2(s,pkg_dir);
					string sgender = "NG";
					string sage = "NG";
					int igender=-1,iage=-1;
					FILE* ffh=fopen("./temp.jpg","wb");
					fwrite((char*)s.data(),s.size(),1,ffh);
					fclose(ffh);
					//getGenderAge((char*)s.data(),s.size(),igender,iage);
					getga(igender,iage);


					if(igender != -1)
					{
						sgender = MaleFemale[igender];
					}
					if(iage != -1)
					{
						//sage = ageStrings[iage];
						char astring[25];
						//itoa(iage,astring,10);
						//char string[25];

// itoa(number, string, 10);
						sprintf(astring, "%d", iage); //换成这一句吧^_^
						sage = astring;
					}

					if(sfuuid=="")//a new face
					{
						SendFace2DB(s.size(),(char*)s.data(),size,(char*)afid,splace.c_str(),spos.c_str(),atoi(sworkmode.c_str()),scamname.c_str(),suuid.c_str(),ssti.c_str(),sgender.c_str(),sage.c_str(),1);//save 2 faceall
						SendFace2DB(s.size(),(char*)s.data(),size,(char*)afid,splace.c_str(),spos.c_str(),atoi(sworkmode.c_str()),scamname.c_str(),suuid.c_str(),ssti.c_str(),sgender.c_str(),sage.c_str(),0);//save 2 face						
						char fn[100]={0};
						memset(fn,0,100);
						//mkdir(GetDateString().c_str(),0775);
						//strcpy(fn,(GetDateString()+string("/")+suuid).c_str());
						mkdir(pkg_dir.c_str(),0775);
						strcpy(fn,(pkg_dir+string("/")+suuid).c_str());
						//SaveBinaryFile(afid,size,fn);
						SaveBinaryFile(s.data(),s.size(),fn);
					}
					else
					{
						std::vector<string> vallfaces;
						LOG4CXX_TRACE(logger,"begin QuerySameAfidFaces");
						//QuerySameAfidFaces(vallfaces,sfuuid);
						LOG4CXX_TRACE(logger,"end QuerySameAfidFaces");
						LOG4CXX_TRACE(logger,"begin UpdateAfid");
						pkg_dir = sfuuid.substr(0,12);
						string su_found = sfuuid.substr(13,36);
						sfuuid = su_found;

						LOG4CXX_TRACE(logger,"pkg_dir = " <<pkg_dir);
						LOG4CXX_TRACE(logger,"sfuuid = "<<sfuuid);

						UpdateAfid(vallfaces,sfuuid,atoi(ssti.c_str()),pkg_dir);
						LOG4CXX_TRACE(logger,"end UpdateAfid");
						LOG4CXX_TRACE(logger,"begin SendFace2DB");
						memset(sprvgender,0,20);
						memset(sprvage,0,20);
						getdbgenderage(su_found);
						//SendFace2DB(s.size(),(char*)s.data(),size,(char*)afid,splace.c_str(),spos.c_str(),atoi(sworkmode.c_str()),scamname.c_str(),sfuuid.c_str(),ssti.c_str(),sgender.c_str(),sage.c_str(),1);//save 2 faceall	
						SendFace2DB(s.size(),(char*)s.data(),size,(char*)afid,splace.c_str(),spos.c_str(),atoi(sworkmode.c_str()),scamname.c_str(),sfuuid.c_str(),ssti.c_str(),sprvgender,sprvage,1);//save 2 faceall	
						LOG4CXX_TRACE(logger,"end SendFace2DB");
						vallfaces.clear();
						std::vector<string>().swap(vallfaces);

					}

					// AFIDReleaseAfid(afid);
					break;
				}
				else
				{
					//LOG4CXX_TRACE(logger,"failed " << res <<" 88 face count "<<facesCollection.nFaces);
					break;
				}

			}

			break;
		}
		LOG4CXX_TRACE(logger,"before releaseimg");
		// if(img!=0)
			// AFIDReleaseImage(img);
		

		session.acknowledge();
		receiver.close();
		connection.close();
 

		// for (unsigned int i = 0; i < nImages; i++)
			// AFIDReleaseFaces(faces[i]);
	//	free(faces);
	//	free(facesCollection.faces);
		LOG4CXX_TRACE(logger,"end releaseimg");

		return 0;
	} catch(const std::exception& error) {
		LOG4CXX_TRACE(logger,error.what());
		connection.close();
	}
	return 1;   
}
/*
void *engine2 = 0;
char *trimwhitespace(char *str)
{
	char *end;
	while(isspace(*str)) str++;
	if(*str == 0)  // All spaces?
        return str;
	// Trim trailing space
	 end = str + strlen(str) - 1;
	while(end > str && isspace(*end)) end--;
	*(end+1) = 0;
        return str;
}
char enginePath[100]={0};
int  getGenderAge(char* img,int size,int &igender,int &iage)
{
	int res;

	if(engine2==0)
	{
		FILE *iFile = fopen("aga.cfg", "r");
		if(!iFile)
		{
        	//printf("Cannot read config file: aga.ini\n");
        	LOG4CXX_TRACE(logger,"Cannot read config file: aga.ini");
		}
		fgets(enginePath, 100, iFile);
		fclose(iFile);
		printf("%s,%s\n",enginePath,trimwhitespace(enginePath));

		SAFE_CALL(AGAInitialize(trimwhitespace(enginePath), &engine2), "AGAInitialize");
	}
	AynxImage *img0 = 0;
	AynxFaces *faces = 0;
	AynxGenderEx gender;
	AynxAge age;
	FILE * ftmp = fopen("temp.jpg","wb");
	fwrite(img,size,1,ftmp);
	fclose(ftmp);

	SAFE_CALL(AGALoadImage("temp.jpg", &img0), "AGALoadImage");
	SAFE_CALL(AGADetectLargestFace(engine2, img0, &faces), "AGADetectLargestFace");	

	if(faces->nFaces > 0)
    {
		//printf("Detected face: (%d, %d) (%dx%d)\n\n", faces->faces[0].location.x, faces->faces[0].location.y, faces->faces[0].location.w, faces->faces[0].location.h);
		//printf("Detected face: (%d, %d) (%dx%d)\n\n", faces->faces[0].location.x, faces->faces[0].location.y, faces->faces[0].location.w, faces->faces[0].location.h);
		SAFE_CALL(AGAPreprocessFaces(engine2, faces), "AGAPreprocessFaces");
        SAFE_CALL(AGAEstimageGenderEx(engine2, faces, &gender), "AGAEstimageGenderEx");
        SAFE_CALL(AGAEstimageAge(engine2, faces, &age), "AGAEstimageAge");

        //printf("Gender estimate: %.2f%% male, %.2f%% female\n", gender.pMale * 100, gender.pFemale * 100);
        LOG4CXX_TRACE(logger,"Gender estimate: " << gender.pMale * 100 <<" male, "<<gender.pFemale * 100 <<" female");
        //printf("Age estimate: %s\n", ageStrings[age-1]);
        LOG4CXX_TRACE(logger,"Age estimate: " << ageStrings[age-1]);

        igender = gender.pMale>gender.pFemale?0:1;
        iage = age-1;
    }
    else
    {
    	LOG4CXX_TRACE(logger,"Failed 2 get face for genderage");
    }
    SAFE_CALL(AGAReleaseFaces(faces), "AGAReleaseFaces");
    SAFE_CALL(AGAReleaseImage(img0), "AGAReleaseImage");
    //SAFE_CALL(AGAFinalize(engine), "AGAFinalize");
    return 0;

}
//*/


int main(int argc, char** argv)
{

	
	PropertyConfigurator::configure("ta_rv2db_logconfig.cfg"); 
	logger = Logger::getLogger("Trace_rv2db"); 
	
	LOG4CXX_TRACE(logger,"run rv2db\n");
	
	InitializeEngine(lib_name);
	void * state = NULL;             // pointer to EyeFace state
    if (!(state = fcnEfInitEyeFace(EYEFACE_DIR,EYEFACE_DIR,EYEFACE_DIR,CONFIG_INI))) 
    {
        fprintf(stderr,"ERROR 103: efInitEyeFace() failed.\n");
        LOG4CXX_TRACE(logger,"ERROR 103: efInitEyeFace() failed.\n");
        freeDll();
        return 103;
    }

//	int ig=-1,ia=-1;
//	getGenderAge(0,0,ig,ia);
//	return 0;

	//read config
	ifstream is("./rvdb_config.xml");
	SeT t=readset(is);
	is.close();

	//printf("%d\n",t.size());
/*
	string sql_server;

	string sql_user;
	string sql_passwd;
	string sql_db;
	string qpid_server;
	string qpid_address;
//*/
	if(t.size()>0)
	{
		
		strcpy(sql_server,t.at(0).sql_server.c_str());
		LOG4CXX_TRACE(logger, sql_server); 
		strcpy(sql_user,t.at(0).sql_user.c_str());
		LOG4CXX_TRACE(logger, sql_user);
		strcpy(sql_passwd,t.at(0).sql_passwd.c_str());
		LOG4CXX_TRACE(logger, sql_passwd);
		strcpy(sql_db,t.at(0).sql_db.c_str());
		LOG4CXX_TRACE(logger, sql_db);
		strcpy(qpid_server,t.at(0).qpid_server.c_str());
		LOG4CXX_TRACE(logger, qpid_server);
		strcpy(qpid_address,t.at(0).qpid_address.c_str());
		LOG4CXX_TRACE(logger, qpid_address);
		DAYS =  atoi(t.at(0).days.c_str());
		LOG4CXX_TRACE(logger,"compare days " <<DAYS);
		FV = (float)atoi(t.at(0).fv.c_str())/100;
		LOG4CXX_TRACE(logger,"FV " <<FV);


	}
	else
	{
		LOG4CXX_TRACE(logger, _T("rv2db no config file")); 
		return 0;
	}

	//memset(sprvgender,0,20);
	//memset(sprvage,0,20);

	//getdbgenderage(string(argv[1]));
	//printf("%s,%s\n",sprvgender,sprvage);
	//return 0;

	mkdir(GetDateString().c_str(),0775);
	LOG4CXX_TRACE(logger, GetDateString()); 

	char ffnn[20]={0};
//	sprintf(ffnn,"%s","afid2.conf");
//	engine = InitializeEngine(ffnn);
	//Readbdata();

/*
	unsigned int mode = 1;
	int res = AFIDSetParam(engine, AYNX_AFID_ENGINE_PARAM_FR_MODE, &mode);
	if (res != AYNX_OK) {
		LOG4CXX_TRACE(logger, _T("failed to set recognition mode ") << res); 
		LOG4CXX_TRACE(logger, _T("rv2db exit")); 
		exit(1);
	}
//*/


	while(1)
	{
		main2(argc,argv);
	}
}
