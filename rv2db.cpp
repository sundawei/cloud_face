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
#include <opencv/cv.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

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
using namespace cv;


#define _TCHAR char
#define _tprintf printf
#define _tfopen fopen
#define _fgetts fgets
#define _tcsstr strstr
#define _T(x) x
#ifndef _MAX_PATH
#define _MAX_PATH       1024
#endif
// min function
//#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
// path to a directory containing config.ini file and models
#define EYEFACE_DIR   	(char*)"/opt/Release-EyeFaceSDK-v3.12.0-CentOS-6.6-x86_64-hasp/eyefacesdk"
#define CONFIG_INI 	    (char*)"config.ini"
// List of testing images
#define VERTICAL_FLIP 0
#define NUM_IMG 150
#ifndef _MAX_PATH
#define _MAX_PATH 1024
#endif

typedef longlong int64;

const char *ageStrings[] = { "0-10", "5-15", "10-20", "15_25", "20-30", "25_35", "30-40", "35-45", "40-50", "45+" };
const char *MaleFemale[] = { "male", "female" };
char sql_server[30] = { 0 };
char sql_user[30] = { 0 };
char sql_passwd[30] = { 0 };
char sql_db[30] = { 0 };
char qpid_server[256] = { 0 };
char qpid_address[256] = { 0 };
float FV = 0.0f;
int n = 0;
//deal with interference data
int has_bdata = 0;
void** bdata_afids = 0;
int bdata_counts = 0;
int DAYS = 0;//diff days before
//set later incoming person's gender age 2 the old one
char sprvgender[20] = { 0 };
char sprvage[20] = { 0 };
LoggerPtr logger = 0;
void *engine = 0;
char lib_name[] = "/opt/Release-EyeFaceSDK-v3.12.0-CentOS-6.6-x86_64-hasp/eyefacesdk/lib/libeyefacesdk-x86_64.so";

/*
class Ddata
{
public:
double *m_iddata;
unsigned int m_idlen;
int m_gender;
int m_age;
public:
Ddata(double* iddata,unsigned int idlen,int gender,int age)
{
m_iddata = new double[]
}
};
//*/





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

typedef vector<R_Setting> SeT;
SeT readset(std::ifstream & is)//read server settings
{
	using boost::property_tree::ptree;
	ptree pt;
	read_xml(is, pt);
	SeT ans;
	BOOST_FOREACH(ptree::value_type const& v, pt.get_child("servers"))
	{
		if (v.first == "server")
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
fcn_efMain                           fcnEfMain;

// Explicit linking - needed for Sentinel LDK (aka HASP) software protection cyphered shared libraries
int loadDll(const char * dll_filename)
{
	EF_OPEN_SHLIB(dll_handle, dll_filename);
	if (!dll_handle)
	{
		printf("%s\n", dll_filename);
		LOG4CXX_TRACE(logger, "Load dll failed ");
		return 1;
	}


	EF_LOAD_SHFCN(fcnEfMain, fcn_efMain, dll_handle, "efMain");
	if (!fcnEfMain)
		return 29;

	// link efReadImageUC()
	EF_LOAD_SHFCN(fcnEfReadImageUc, fcn_efReadImageUc, dll_handle, "efReadImageUc");
	if (!fcnEfReadImageUc)
		return 2;

	// link efFreeImageUC()
	EF_LOAD_SHFCN(fcnEfFreeImageUc, fcn_efFreeImageUc, dll_handle, "efFreeImageUc");
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
void GetRawImage(unsigned char*& raw_img, string jpgdata)
{
	std::vector<char> data1((char*)jpgdata.data(), (char*)jpgdata.data() + jpgdata.size());
	cv::Mat imgMat = cv::imdecode(Mat(data1), CV_LOAD_IMAGE_COLOR);
	IplImage img = imgMat;
	IplImage *grey_image = cvCreateImage(cvSize(img.width, img.height), 8, 1);
	// convert opencv image to gray image and to raw_img
	cvCvtColor(&img, grey_image, CV_BGR2GRAY);
	raw_img = new unsigned char[img.width*img.height];
	// copy opencv image to raw image
	int cnt = 0;
	for (int i = 0; i < grey_image->height; i++)
	for (int j = 0; j < grey_image->width; j++)
		raw_img[cnt++] = ((unsigned char*)(grey_image->imageData + i*grey_image->widthStep))[j];

	cvReleaseImage(&grey_image);
	data1.clear();
	std::vector<char>().swap(data1);

}
string GetDateString()//get date dir
{
	time_t now;
	struct tm *tm_now;
	char sdatetime[20];

	time(&now);
	tm_now = localtime(&now);
	strftime(sdatetime, 20, "./%Y-%m-%d", tm_now);
	return std::string(sdatetime);
}

int finish_with_error(MYSQL *con)//close mysql error
{
	LOG4CXX_TRACE(logger, "mysql error " << mysql_error(con));
	mysql_close(con);
	return 0;
}

int SaveBinaryFile(const void* data, unsigned int size, _TCHAR* name)
{
	FILE* fp = _tfopen(name, _T("wb"));
	if (fp == 0) {
		LOG4CXX_TRACE(logger, "failed to create " << name);
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
		LOG4CXX_TRACE(logger, "out of memory " << name);
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


void getbdirs(long tttm, int days, vector<string>& salldirs)//find available date dirs
{
	for (int i = 0; i<days; i++)
	{
		long atm = tttm - i * 24 * 3600;
		struct tm *tm_atm = localtime(&atm);
		char adir[16] = { 0 };
		strftime(adir, 16, "./%Y-%m-%d", tm_atm);
		DIR   *dp;
		if ((dp = opendir(adir)) == NULL)
		{
			LOG4CXX_TRACE(logger, " " << adir << " not exists");
		}
		else
		{
			LOG4CXX_TRACE(logger, "add a date dir " << adir);
			salldirs.push_back(adir);
		}
		closedir(dp);

	}
}
void getdirfiles(const char* sdir, unsigned int & count, vector<string>& fname)//get filelist from dir
{
	std::vector<string> vdirs;
	getbdirs(time(0), DAYS, vdirs);
	count = 0;
	if (vdirs.size() == 0 || strcmp(sdir, "./bdata") == 0)//bddir spec
	{

		DIR   *dp;
		struct   dirent   *dirp;
		if ((dp = opendir(sdir)) == NULL)
		{
			LOG4CXX_TRACE(logger, "no dir " << sdir);
			count = 0;
			return;
		}
		int ret = 0;
		while ((dirp = readdir(dp)) != NULL)
		{

			if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
				continue;
			string sall = string(sdir) + string("/") + string(dirp->d_name);
			fname.push_back(sall);
			LOG4CXX_TRACE(logger, "push file " << sall);
			ret++;
		}
		count += ret;
		closedir(dp);
	}
	else
	{
		for (int i = 0; i<static_cast<int>(vdirs.size()); i++)
		{
			DIR   *dp;
			struct   dirent   *dirp;
			if ((dp = opendir(vdirs.at(i).c_str())) == NULL)
			{
				LOG4CXX_TRACE(logger, "no dir " << sdir);
				count = 0;
				closedir(dp);
				continue;
			}
			int ret = 0;
			while ((dirp = readdir(dp)) != NULL)
			{

				if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
					continue;

				string snm = string(dirp->d_name);
				if (snm.substr(strlen(dirp->d_name) - 3, 3) != string(".id"))
					continue;

				string sall = vdirs.at(i) + string("/") + string(dirp->d_name);
				fname.push_back(sall);
				LOG4CXX_TRACE(logger, "push file " << sall);
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
	getdirfiles("./bdata", count, vbdatafilename);
	if (count>0)
	{
		has_bdata = 1;
		bdata_counts = count;
		bdata_afids = (void **)malloc(count * sizeof(void *));
		memset(bdata_afids, 0, count * sizeof(void *));
		for (unsigned int i = 0; i < count; i++) {
			unsigned int afidSize = 0;
			void* pAfid = 0;
			char onefile[256] = { 0 };
			strcpy(onefile, vbdatafilename.at(i).c_str());
			int res = ReadBinaryFile(onefile, &pAfid, &afidSize);
			if (res != 0) {
				LOG4CXX_TRACE(logger, _T("failed to read ") << vbdatafilename.at(i).c_str());
				std::vector<string>().swap(vbdatafilename);
				return;
			}
			bdata_afids[i] = pAfid;
		}
		LOG4CXX_TRACE(logger, "found bdata counts =  " << bdata_counts);
	}
}
string DealBadData(string facedata)//verify wether spec face is not a face
{
	return "";
}
int GetFileFacID(EfDetResult * &det_result_1, char* filename)
{
	void * state = NULL;
	EfMatrixUc * input_image_1 = NULL;

	//printf("05\n");
	if (!(state = fcnEfInitEyeFace(EYEFACE_DIR, EYEFACE_DIR, EYEFACE_DIR, CONFIG_INI)))
	{
		fprintf(stderr, "ERROR 103: efInitEyeFace() failed.\n");
		return 103;
	}
	//printf("06\n");
	// printf("filename %s\n",filename);

	if (!(input_image_1 = fcnEfReadImageUc(filename)))
	{
		LOG4CXX_TRACE(logger, "ERROR 105: Can't load the image file");
		return 105;
	}

	if (!(det_result_1 = fcnEfRunFaceDetector(input_image_1, state)))
	{
		LOG4CXX_TRACE(logger, "ERROR 106: efRunFaceDetector() failed.");
		return 106;
	}
	if (det_result_1->num_detections<1)
	{
		fcnEfFreeImageUc(input_image_1);
		fcnEfShutdownEyeFace(state);
		fcnEfClearEyeFaceState(state);
		fcnEfFreeEyeFaceState(state);
		return 170;//no face
	}
	printf("07\n");
	//*
	for (int i = 0; i < det_result_1->num_detections; i++)
	{
		EfLandmarkResult * landmark_result = NULL;
		if (!(landmark_result = fcnEfRunFaceLandmark(input_image_1, &(det_result_1->detection[i]), state)))
		{
			LOG4CXX_TRACE(logger, "ERROR 107: efRunFaceLandmark() failed.");
			return 107;
		}

		printf("08\n");
		if (landmark_result->error == 1)
			LOG4CXX_TRACE(logger, "Warning: Can't estimate face landmarks, augmented face BB goes out of the image.");
		if (fcnEfRunFaceImageExtraction(input_image_1, &(det_result_1->detection[i]), state, landmark_result))
		{
			LOG4CXX_TRACE(logger, "ERROR 108: Face extraction failed.");
			return 108;
		}
		fcnEfFreeLandmarkResult(landmark_result);
	}
	printf("09\n");
	for (int i = 0; i < det_result_1->num_detections; i++)
	{
		int frontal_face = 0;
		if (det_result_1->detection[i].angles[2] > 0)
			frontal_face = det_result_1->detection[i].angles[2];
		else
			frontal_face = 0 - det_result_1->detection[i].angles[2];
		frontal_face = frontal_face < 30; // only compute face attributes for frontal faces!!!
		if (frontal_face)
		{
			if (fcnEfRunFaceIDFeatureExtraction(&(det_result_1->detection[i]), state) == -1)
			{
				fprintf(stderr, "ERROR 112: Face ID features computation failed.\n");
				return 112;
			}
		}
	}
	printf("10\n");
	//*/
	fcnEfFreeImageUc(input_image_1);
	fcnEfShutdownEyeFace(state);
	fcnEfClearEyeFaceState(state);
	fcnEfFreeEyeFaceState(state);
	return 1;
}
unsigned int  SaveIDFile(EfDetResult* &det, string filename)
{
	unsigned int feat_A_length = 0;
	printf("begin save ID\n");
	if (det == NULL)
	{
		printf("NULL det return\n");
		return 0;
	}
	if (det->num_detections)
	{
		LOG4CXX_TRACE(logger, "num detections " << det->num_detections);
		//printf()
		feat_A_length = det->detection[0].feat_id_length;
		LOG4CXX_TRACE(logger, "Dummy copying of ID feature length =  " << feat_A_length);
		double * feat_A = new double[feat_A_length];
		memcpy(feat_A, det->detection[0].feat_id, sizeof(double)*feat_A_length);
		LOG4CXX_TRACE(logger, filename);
		FILE *fp = fopen(filename.c_str(), "w");
		fwrite(feat_A, sizeof(double)*feat_A_length, 1, fp);
		fclose(fp);

		delete[] feat_A;
		LOG4CXX_TRACE(logger, "done.\n");
	}
	else
	{
		LOG4CXX_TRACE(logger, "det num_detections == 0\n");
	}
	return feat_A_length;
}
int GetFaceID(EfDetResult * &det_result_1, string facedata)
{
	void * state = NULL;
	EfMatrixUc * input_image_1 = NULL;
	FILE* ffh = fopen("./temp1.jpg", "wb");
	fwrite((char*)facedata.data(), facedata.size(), 1, ffh);
	fflush(ffh);
	fclose(ffh);

	//GetRawImage((unsigned char*&)input_image_1,facedata);
	//printf("01\n");
	if (!(state = fcnEfInitEyeFace(EYEFACE_DIR, EYEFACE_DIR, EYEFACE_DIR, CONFIG_INI)))
	{
		fprintf(stderr, "ERROR 103: efInitEyeFace() failed.\n");
		printf("ERROR 103: efInitEyeFace() failed.\n");
		LOG4CXX_TRACE(logger, "ERROR 103: efInitEyeFace() failed.\n");
		return 103;
	}
	//printf("01.5\n");
	if (!(input_image_1 = fcnEfReadImageUc("./temp1.jpg")))
	{
		LOG4CXX_TRACE(logger, "ERROR 105: Can't load the image file");
		printf("ERROR 105: Can't load the image file");
		return 105;
	}
	//printf("02\n");
	if (!(det_result_1 = fcnEfRunFaceDetector(input_image_1, state)))
	{
		LOG4CXX_TRACE(logger, "ERROR 106: efRunFaceDetector() failed.");
		LOG4CXX_TRACE(logger, "ERROR 106: efRunFaceDetector() failed.");
		return 106;
	}
	//printf("03\n");
	if (det_result_1->num_detections<1)
	{
		fcnEfFreeImageUc(input_image_1);
		fcnEfShutdownEyeFace(state);
		fcnEfClearEyeFaceState(state);
		fcnEfFreeEyeFaceState(state);
		LOG4CXX_TRACE(logger, "sdk found 0 face");
		return 170;//no face
	}
	//*
	for (int i = 0; i < det_result_1->num_detections; i++)
	{
		EfLandmarkResult * landmark_result = NULL;
		if (!(landmark_result = fcnEfRunFaceLandmark(input_image_1, &(det_result_1->detection[i]), state)))
		{
			LOG4CXX_TRACE(logger, "ERROR 107: efRunFaceLandmark() failed.");
			return 107;
		}

		if (landmark_result->error == 1)
			LOG4CXX_TRACE(logger, "Warning: Can't estimate face landmarks, augmented face BB goes out of the image.");
		if (fcnEfRunFaceImageExtraction(input_image_1, &(det_result_1->detection[i]), state,
			landmark_result))
		{
			LOG4CXX_TRACE(logger, "ERROR 108: Face extraction failed.");
			return 108;
		}
		printf("04\n");
		fcnEfFreeLandmarkResult(landmark_result);
	}
	for (int i = 0; i < det_result_1->num_detections; i++)
	{
		int frontal_face = 0;
		if (det_result_1->detection[i].angles[2]>0)
			frontal_face = det_result_1->detection[i].angles[2];
		else
			frontal_face = 0 - det_result_1->detection[i].angles[2];
		frontal_face = frontal_face < 30.0; // only compute face attributes for frontal faces!!!
		if (frontal_face)
		{
			if (fcnEfRunFaceIDFeatureExtraction(&(det_result_1->detection[i]), state) == -1)
			{
				fprintf(stderr, "ERROR 112: Face ID features computation failed.\n");
				return 112;
			}
		}
	}
	//*/
	fcnEfFreeImageUc(input_image_1);
	fcnEfShutdownEyeFace(state);
	fcnEfClearEyeFaceState(state);
	fcnEfFreeEyeFaceState(state);
	return 1;

}

float CompareFace(double* &e1, double* &e2)
{
	void * state = NULL;
	if (!(state = fcnEfInitEyeFace(EYEFACE_DIR, EYEFACE_DIR, EYEFACE_DIR, CONFIG_INI)))
	{
		fprintf(stderr, "ERROR 103: efInitEyeFace() failed.\n");
		return 103;
	}
	//if(e1->num_detections>0&&e2->num_detections>0)
	//{
	double response = 0.0;
	fcnEfIDCompareFeatureVectors(e1, e2, state, &response);
	fcnEfShutdownEyeFace(state);
	fcnEfClearEyeFaceState(state);
	fcnEfFreeEyeFaceState(state);
	return response;
	//}
	//else
	//{
	//	 fcnEfShutdownEyeFace(state);
	//	 fcnEfClearEyeFaceState(state);
	//	 fcnEfFreeEyeFaceState(state);

	//	return 0.0f;
	//}
}
long GetFileSize(char *filename)
{

	long siz = 0;

	FILE *fp = fopen(filename, "rb");

	if (fp) {

		fseek(fp, 0, SEEK_END);

		siz = ftell(fp);

		fclose(fp);

	}
	return siz;
}

string FindMatchFace2(string facedata, string pkgdir, int gender, int age)
{
	string rets = "";
	EfDetResult *det_result_1;
	if (1 != GetFaceID(det_result_1, facedata))
	{
		fcnEfFreeDetResult(det_result_1);
		return "";
	}
	std::vector<string> v;
	unsigned int nAfids = 0;
	char sdir[20] = { 0 };
	strcpy(sdir, pkgdir.c_str());
	getdirfiles(sdir, nAfids, v);
	unsigned int i = 0;
	for (i = 0; i < nAfids; i++)
	{
		char onefile[256] = { 0 };
		strcpy(onefile, v.at(i).c_str());
		int slen = strlen(onefile);
		if (onefile[slen - 1] != 'd' || onefile[slen - 2] != 'i' || onefile[slen - 3] != '.')
		{
			continue;
		}
		//033E77F0-F690-E292-DB8A-8CEC00000000_0_36.id
		int fgender = -1, fage = -1;
		int ist = strlen(v.at(i).c_str()) - 8;
		fgender = atoi(v.at(i).substr(ist + 1, 1).c_str());
		fage = atoi(v.at(i).substr(ist + 3, 2).c_str());


		if (fgender != gender)
			continue;
		if (abs(fage - age)>5)
			continue;
		LOG4CXX_TRACE(logger, "compare a face age0=" << age << " age1=" << fage << " gender0=" << gender << " gender1=" << fgender);

		int len = GetFileSize(onefile);
		if (len == 0)
			continue;
		double * cnt = new double[len / sizeof(double)];
		FILE *ft = fopen(onefile, "rb");
		fread(cnt, len, 1, ft);
		fclose(ft);
		float rate = CompareFace(det_result_1->detection[0].feat_id, cnt);
		delete[] cnt;
		if (rate > 2.7f)
		{
			rets = v.at(i);
			LOG4CXX_TRACE(logger, "found same face " << v.at(i));
			break;
		}
	}
	LOG4CXX_TRACE(logger, "total compare " << i << " faces");
	fcnEfFreeDetResult(det_result_1);
	return rets;
}

int SendFace2DB(int len0, char* facedata, int len1, char* afid, const char* place, const char* pos, int workmode, const char* camname, const char* suuid, const char* stime, const char* sgender, const char* sage, int whichdb = 0)
//whichdb 0:face 1:faceall
{
	int size = len0;
	int size2 = len1;
	MYSQL *con = mysql_init(NULL);
	if (con == NULL)
	{
		LOG4CXX_TRACE(logger, "mysql_init() failed");
		return 0;
	}
	if (mysql_real_connect(con, sql_server, sql_user, sql_passwd, sql_db, 0, NULL, 0) == NULL)
	{
		return finish_with_error(con);
	}


	char *chunk = new char[2 * size + 1];
	char *chunk2 = new char[2 * size2 + 1];
	mysql_real_escape_string(con, chunk, facedata, size);
	mysql_real_escape_string(con, chunk2, afid, size2);
	const char *st = "INSERT INTO face(face, afid, dt, afidlen, place, pos, workmode, camname, uuid, gender, age) VALUES('%s', '%s',FROM_UNIXTIME(%d),'%d','%s','%s','%d','%s','%s','%s','%s')";
	const char *st1 = "INSERT INTO faceall(face, afid, dt, afidlen, place, pos, workmode, camname, uuid, gender, age) VALUES('%s', '%s',FROM_UNIXTIME(%d),'%d','%s','%s','%d','%s','%s','%s','%s')";
	size_t st_len;
	if (whichdb == 0)
	{
		st_len = strlen(st);
	}
	else
	{
		st_len = strlen(st1);
	}

	char *query = new char[st_len + 2 * size + 1 + 100 + 40 + 80 + 2 * size2 + 1];
	int len = 0;
	if (whichdb == 0)
		len = snprintf(query, st_len + 2 * size + 1 + 100 + 40 + 80 + 2 * size2 + 1, st, chunk, chunk2, atoi(stime), size2, place, pos, workmode, camname, suuid, sgender, sage);
	else
		len = snprintf(query, st_len + 2 * size + 1 + 100 + 40 + 80 + 2 * size2 + 1, st1, chunk, chunk2, atoi(stime), size2, place, pos, workmode, camname, suuid, sgender, sage);

	LOG4CXX_TRACE(logger, "insert into db gender=" << sgender << " age=" << sage << " db=" << whichdb);
	if (mysql_real_query(con, query, len))
	{
		delete[] chunk;
		delete[] chunk2;
		delete[] query;
		return finish_with_error(con);
	}
	mysql_close(con);
	delete[] chunk;
	delete[] chunk2;
	delete[] query;
	return 1;
}

void UpdateDBAfid(string suuid, string safiddata, int ntime)//change afid of a face
{
	MYSQL *con = mysql_init(NULL);
	int sfaidlen = suuid.size();
	string cs = suuid.substr(sfaidlen - 36, 36);
	if (con == NULL)
	{
		LOG4CXX_TRACE(logger, "mysql_init() failed");
		return;
	}
	if (mysql_real_connect(con, sql_server, sql_user, sql_passwd, sql_db, 0, NULL, 0) == NULL)
	{
		finish_with_error(con);
		return;
	}
	char *chunk1 = new char[2 * safiddata.size() + 1];
	mysql_real_escape_string(con, chunk1, safiddata.data(), safiddata.size());
	const char* qs = "UPDATE face SET afid='%s', ldt=FROM_UNIXTIME(%d) WHERE uuid='%s'";
	size_t st_len = strlen(qs);
	char *query = new char[st_len + 2 * safiddata.size() + 1 + 80];
	int len = snprintf(query, st_len + 2 * safiddata.size() + 1 + 80, qs, chunk1, ntime, cs.c_str());
	if (mysql_real_query(con, query, len))
	{
		delete[] chunk1;
		delete[] query;
		finish_with_error(con);
		return;
	}
	mysql_close(con);
	delete[] chunk1;
	delete[] query;
}

void QuerySameAfidFaces(vector<string>& allfaces, string suuid)
{
	MYSQL *con = mysql_init(NULL);
	int sfaidlen = suuid.size();
	string cs = suuid.substr(sfaidlen - 36, 36);
	if (con == NULL)
	{
		LOG4CXX_TRACE(logger, "mysql_init() failed");
		return;
	}
	if (mysql_real_connect(con, sql_server, sql_user, sql_passwd, sql_db, 0, NULL, 0) == NULL)
	{
		finish_with_error(con);
		return;
	}
	string querys = "SELECT Id,face FROM faceall WHERE uuid ='" + cs + "'";
	if (mysql_query(con, querys.c_str()))
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
	int tc = 0;
	while ((row = mysql_fetch_row(result)))
	{
		unsigned long *lengths;
		lengths = mysql_fetch_lengths(result);
		if (lengths == NULL)
		{
			break;
		}
		for (int i = 0; i < num_fields; i++)
		{
			if (i == 1)
			{
				string sp;
				sp.assign(row[i], lengths[i]);
				allfaces.push_back(sp);
			}
		}
		tc++;
		if (tc >= 30)
			break;
	}
	mysql_free_result(result);
	mysql_close(con);
}

void UpdateAfid(vector<string>& allfaces, string suuid, int newfacetime, string pkgdir)
{
	string safiddata = "";
	UpdateDBAfid(suuid, safiddata, newfacetime);
}


int getga2(IplImage img, int& g, int& a)
{
	/*
	ef_shlib_hnd dll_handle2;                                                  // global pointer to EyeFace-SDK shared library

	EF_OPEN_SHLIB(dll_handle2,lib_name);

	fcn_efInitEyeFace                       fcnEfInitEyeFace1;
	fcn_efFreeEyeFaceState                  fcnEfFreeEyeFaceState1;
	fcn_efMain                              fcnEfMain1;
	fcn_efGetVisualOutput                   fcnEfGetVisualOutput1;
	fcn_efFreeVisualOutput                  fcnEfFreeVisualOutput1;

	EF_LOAD_SHFCN(fcnEfInitEyeFace1, fcn_efInitEyeFace, dll_handle2, "efInitEyeFace");
	EF_LOAD_SHFCN(fcnEfFreeEyeFaceState1, fcn_efFreeEyeFaceState, dll_handle2, "efFreeEyeFaceState");
	EF_LOAD_SHFCN(fcnEfMain1, fcn_efMain, dll_handle2, "efMain");
	EF_LOAD_SHFCN(fcnEfGetVisualOutput1, fcn_efGetVisualOutput, dll_handle2, "efGetVisualOutput");
	EF_LOAD_SHFCN(fcnEfFreeVisualOutput1, fcn_efFreeVisualOutput, dll_handle2, "efFreeVisualOutput");
	//*/
	IplImage *grey_image = cvCreateImage(cvSize(img.width, img.height), 8, 1);

	// convert opencv image to gray image and to raw_img
	cvCvtColor(&img, grey_image, CV_BGR2GRAY);

	unsigned char * raw_img = new unsigned char[img.width*img.height];
	// copy opencv image to raw image
	int cnt = 0;
	for (int i = 0; i < grey_image->height; i++)
	for (int j = 0; j < grey_image->width; j++)
		raw_img[cnt++] = ((unsigned char*)(grey_image->imageData + i*grey_image->widthStep))[j];

	// init EyeFace state
	void * state;
	state = fcnEfInitEyeFace(EYEFACE_DIR, EYEFACE_DIR, EYEFACE_DIR, CONFIG_INI);

	// EyeFace SDK init failed? (license invalid)
	if (!state)
	{
		cvReleaseImage(&grey_image);
		delete[] raw_img;
		printf("getga2 failed 1\n");
		//EF_FREE_LIB(dll_handle2);
		LOG4CXX_TRACE(logger, "getga2 failed");
		return -1;
	}

	// setup detection area
	EfBoundingBox bounding_box;
	bounding_box.top_left_col = 0;
	bounding_box.top_left_row = 0;
	bounding_box.bot_right_col = grey_image->width - 1;
	bounding_box.bot_right_row = grey_image->height - 1;

	// run face detection and recognition
	fcnEfMain(raw_img, grey_image->width, grey_image->height, 0, &bounding_box, state, 0);

	cvReleaseImage(&grey_image);
	delete[] raw_img;
	// recover tracks from the state
	EfVisualOutput * visual_output = fcnEfGetVisualOutput(state, 0, 0);
	//    printf("Number of detected faces: %d\n", visual_output->num_vis);

	for (int i = 0; i < visual_output->num_vis; i++)
	{
		EfGender gender = visual_output->vis_data[i].gender;
		printf("%s\n",
			(gender == EF_GENDER_NONE ? "-1" :
			(gender == EF_GENDER_MALE ? "0" : "1")));
		if (gender == EF_GENDER_NONE)
			g = -1;
		else if (gender == EF_GENDER_MALE)
			g = 0;
		else
			g = 1;

		if (visual_output->vis_data[i].age >= 0)
			printf("%d\n", visual_output->vis_data[i].age);
		else
			printf("%s\n", "-1");
		a = visual_output->vis_data[i].age;

		printf("********************\n");
		printf("age = %d gender = %d\n", a, g);
		printf("********************\n");
		break;
	}

	// Tidy-up
	fcnEfFreeVisualOutput(visual_output);
	fcnEfShutdownEyeFace(state);
	fcnEfClearEyeFaceState(state);
	fcnEfFreeEyeFaceState(state);
	//fcnEfFreeEyeFaceState(state);
	// EF_FREE_LIB(dll_handle2);

	return 0;
}
void getga(int& g, int& a)
{
	FILE *fp = popen("./getGenderAge2", "r");
	if (fp == NULL)
		return;
	char buf[10] = { 0 };

	int count = 0;
	LOG4CXX_TRACE(logger, "before getga");
	while (fgets(buf, 10, fp) != NULL)
	{
		count++;
		if (count == 1)
			g = atoi(buf);
		if (count == 2)
		{
			a = atoi(buf);
		}
	}
	LOG4CXX_TRACE(logger, "end getga");
	if (pclose(fp) == -1)
	{
		LOG4CXX_TRACE(logger, "error pclose " << errno);
	}
}


void getdbgenderage(string suuid)//search db 2 find first rec gender and age
{
	MYSQL *con = mysql_init(NULL);
	string retstr = "";
	string cs = suuid;
	if (con == NULL)
	{
		LOG4CXX_TRACE(logger, "mysql_init() failed");
		return;
	}
	if (mysql_real_connect(con, sql_server, sql_user, sql_passwd, sql_db, 0, NULL, 0) == NULL)
	{
		finish_with_error(con);
		return;
	}
	string querys = "SELECT Id,gender,age FROM face WHERE uuid ='" + cs + "'";
	LOG4CXX_TRACE(logger, querys);

	if (mysql_query(con, querys.c_str()))
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
	int tc = 0;
	while ((row = mysql_fetch_row(result)))
	{
		unsigned long *lengths;
		lengths = mysql_fetch_lengths(result);
		if (lengths == NULL)
		{
			break;
		}
		for (int i = 0; i < num_fields; i++)
		{
			if (i == 1)
			{
				memcpy(sprvgender, row[i], lengths[i]);
			}
			if (i == 2)
			{
				memcpy(sprvage, row[i], lengths[i]);
			}
		}
		tc++;
		break;
	}
	mysql_free_result(result);
	mysql_close(con);
}
#define DEALSAMECOUNT 10
//EfDetResult* L_10_result[DEALSAMECOUNT]={NULL};
double* L_10_result[DEALSAMECOUNT] = { 0 };
int Save_10_index = -1;
int DetectSameFace(string sfacenow)
{
	int ret = -1;
	//return ret;
	EfDetResult * det_result_1 = NULL;
	if (1 != GetFaceID(det_result_1, sfacenow))
	{
		if (det_result_1 != NULL)
		{
			LOG4CXX_TRACE(logger, "GetFaceID Failed but detresult not NULL");
			fcnEfFreeDetResult(det_result_1);
		}
		return -1;
	}


	for (int i = 0; i<DEALSAMECOUNT; i++)
	{
		if (L_10_result[i] != NULL)
		{
			float cf = CompareFace(det_result_1->detection[0].feat_id, L_10_result[i]);
			if (cf >= 2.7f)
			{
				//printf("====%f=====%f=====\n",det_result_1->detection[0].feat_id_length,L_10_result[i]->detection[0].feat_id_length);
				//printf("cf = %f \n");
				ret = 1;
				break;
			}
		}
	}

	if (NULL != det_result_1)
		fcnEfFreeDetResult(det_result_1);

	return ret;
}

int GetFreeMemory()
{
	FILE *fd;

	char buff[256];

	int total = 0;

	int free = 0;

	float use = 0;



	fd = fopen("/proc/meminfo", "r");



	fgets(buff, sizeof(buff), fd);

	sscanf(buff, "%*s %d %*s", &total);



	fgets(buff, sizeof(buff), fd);

	sscanf(buff, "%*s %d %*s", &free);

	//printf("free = %d\n",free);
	//use=(float)((total-free) /(total * 0.01));//百分比



	fclose(fd);

	return free / 1024;

}
void GetGAFromPic(string spic, int &g, int &a)
{
	std::vector<char> data1((char*)spic.data(), (char*)spic.data() + spic.size());
	cv::Mat imgMat = cv::imdecode(Mat(data1), CV_LOAD_IMAGE_COLOR);
	IplImage iimg = imgMat;
	getga2(iimg, g, a);
	data1.clear();
	std::vector<char>().swap(data1);
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
		decode(receiver.fetch(Duration::SECOND * 60), content);
		string s = content["picture"];
		string splace = content["place"];
		string spos = content["pos"];
		string sworkmode = content["workmode"];
		string scamname = content["camname"];
		string suuid = content["uuid"];
		string ssti = content["int"];
		time_t now = atoi(ssti.c_str());
		struct tm *tm_now;
		char    sdatetime[50] = { 0 };
		char pkg_date_dir[16] = { 0 };
		tm_now = localtime(&now);
		strftime(sdatetime, 50, "PKG time %Y-%m-%d %H:%M:%S", tm_now);
		strftime(pkg_date_dir, 16, "./%Y-%m-%d", tm_now);
		string pkg_dir = pkg_date_dir;
		printf("%s\n", pkg_dir.c_str());
		LOG4CXX_TRACE(logger, pkg_dir.c_str());
		LOG4CXX_TRACE(logger, " " << tm_now << " " << atoi(ssti.c_str()) << " " << sdatetime);
		while (1)
		{
			//printf("got here\n");
			//*
			EfDetResult * det_result_1;
			if (1 != GetFaceID(det_result_1, s))
			{
				//break;
				if (det_result_1 != NULL)
					fcnEfFreeDetResult(det_result_1);
				det_result_1 = NULL;
				break;//not find face doesn't save
			}
			//fcnEfFreeDetResult(det_result_1);
			if (DetectSameFace(s)<0)
			{
				Save_10_index++;

				if (Save_10_index >= DEALSAMECOUNT)
				{
					Save_10_index = 0;
				}

				if (L_10_result[Save_10_index] != NULL)
				{
					//fcnEfFreeDetResult(L_10_result[Save_10_index]);
					delete[](double*)L_10_result[Save_10_index];
				}

				unsigned int feat_A_length = det_result_1->detection[0].feat_id_length;
				printf("\nDummy copying of ID feature (length = %d)... ", feat_A_length);
				L_10_result[Save_10_index] = new double[feat_A_length];
				memcpy(L_10_result[Save_10_index], det_result_1->detection[0].feat_id, sizeof(double)*feat_A_length);

				//L_10_result[Save_10_index]=det_result_1;
			}
			else
			{
				if (NULL != det_result_1)
					fcnEfFreeDetResult(det_result_1);

				LOG4CXX_TRACE(logger, "Found a similar face or not found a face! Do not save it.");
				break;
			}
			//fcnEfFreeDetResult(det_result_1);
			//*/
			void* afid = 0;
			unsigned int size = 0;
			LOG4CXX_TRACE(logger, "ok");

			clock_t starttime, endtime;
			double totaltime;
			starttime = clock();
			int igender = -1, iage = -1;
			//string sgender = "NG";
			//string sage = "NG";
			GetGAFromPic(s, igender, iage);
			if (iage == -1)
			{
				if (NULL != det_result_1)
					fcnEfFreeDetResult(det_result_1);
				break;
			}

			if (iage != -1 && igender == -1)
			{
				igender = 0;
			}

			string sfuuid = FindMatchFace2(s, pkg_dir, igender, iage);

			endtime = clock();
			totaltime = (double)((endtime - starttime) / (double)CLOCKS_PER_SEC);

			LOG4CXX_TRACE(logger, "@@@@total deal match time " << totaltime << "@@@");
			string sgender = "NG";
			string sage = "NG";



			if (sfuuid == "")
			{
				mkdir(pkg_dir.c_str(), 0775);
				char ga_param[10] = { 0 };
				sprintf(ga_param, "_%d_%02d", igender, iage);
				int len = SaveIDFile(det_result_1, pkg_dir + string("/") + suuid + string(ga_param) + string(".id"));

				LOG4CXX_TRACE(logger, pkg_dir + string("/") + suuid + string(ga_param) + string(".id"));
				if (len == 0)
				{
					if (NULL != det_result_1)
						fcnEfFreeDetResult(det_result_1);
					break;
				}
			}

			LOG4CXX_TRACE(logger, "begin free result");
			if (NULL != det_result_1)
				fcnEfFreeDetResult(det_result_1);
			LOG4CXX_TRACE(logger, "end free result");

			//	std::vector<char> data1((char*)s.data(), (char*)s.data() + s.size());
			//	cv::Mat imgMat = cv::imdecode(Mat(data1), CV_LOAD_IMAGE_COLOR);
			//	IplImage iimg=imgMat;
			//	getga2(iimg,igender,iage);
			//		data1.clear();
			//	std::vector<char>().swap(data1);
			if (igender != -1)
			{
				sgender = MaleFemale[igender];
			}
			else
			{
				sgender = "male";
			}
			if (iage != -1)
			{
				char astring[25];
				sprintf(astring, "%d", iage); //换成这一句吧^_^
				sage = astring;
			}


			if (sfuuid == "")//a new face
			{
				LOG4CXX_TRACE(logger, "add a new face" << sgender << " " << sage);
				SendFace2DB(s.size(), (char*)s.data(), size, (char*)afid, splace.c_str(), spos.c_str(), atoi(sworkmode.c_str()), scamname.c_str(), suuid.c_str(), ssti.c_str(), sgender.c_str(), sage.c_str(), 1);//save 2 faceall
				SendFace2DB(s.size(), (char*)s.data(), size, (char*)afid, splace.c_str(), spos.c_str(), atoi(sworkmode.c_str()), scamname.c_str(), suuid.c_str(), ssti.c_str(), sgender.c_str(), sage.c_str(), 0);//save 2 face
				char fn[100] = { 0 };
				memset(fn, 0, 100);
				mkdir(pkg_dir.c_str(), 0775);
				strcpy(fn, (pkg_dir + string("/") + suuid).c_str());
				SaveBinaryFile(s.data(), s.size(), fn);
			}
			else
			{
				std::vector<string> vallfaces;
				LOG4CXX_TRACE(logger, "begin QuerySameAfidFaces");
				LOG4CXX_TRACE(logger, "end QuerySameAfidFaces");
				LOG4CXX_TRACE(logger, "begin UpdateAfid");
				pkg_dir = sfuuid.substr(0, 12);
				string su_found = sfuuid.substr(13, 36);
				sfuuid = su_found;
				LOG4CXX_TRACE(logger, "pkg_dir = " << pkg_dir);
				LOG4CXX_TRACE(logger, "sfuuid = " << sfuuid);
				UpdateAfid(vallfaces, sfuuid, atoi(ssti.c_str()), pkg_dir);
				LOG4CXX_TRACE(logger, "end UpdateAfid");
				LOG4CXX_TRACE(logger, "begin SendFace2DB");
				memset(sprvgender, 0, 20);
				memset(sprvage, 0, 20);
				getdbgenderage(su_found);
				if (sprvgender != ""&&sprvage != "")
					SendFace2DB(s.size(), (char*)s.data(), size, (char*)afid, splace.c_str(), spos.c_str(), atoi(sworkmode.c_str()), scamname.c_str(), sfuuid.c_str(), ssti.c_str(), sprvgender, sprvage, 1);//save 2 faceall
				LOG4CXX_TRACE(logger, "end SendFace2DB");
				vallfaces.clear();
				std::vector<string>().swap(vallfaces);

			}
			if (GetFreeMemory()<300)
			{
				time_t now;
				struct tm *curTime;
				char filename[256];
				now = time(NULL);
				curTime = localtime(&now);
				sprintf(filename, "%04d-%02d-%02d %02d-%02d-%02d", curTime->tm_year + 1900,
					curTime->tm_mon + 1, curTime->tm_mday, curTime->tm_hour, curTime->tm_min,
					curTime->tm_sec);
				FILE *fp = fopen(filename, "w");
				fprintf(fp, "memory too low ,only %d left", GetFreeMemory());
				fclose(fp);
				system("/root/recovery.sh");
				exit(0);
			}
			break;
		}
		LOG4CXX_TRACE(logger, "before releaseimg");
		session.acknowledge();
		receiver.close();
		connection.close();
		LOG4CXX_TRACE(logger, "end releaseimg");
		return 0;
	}
	catch (const std::exception& error) {
		LOG4CXX_TRACE(logger, error.what());
		connection.close();
	}
	return 1;
}

int main(int argc, char** argv)
{
	PropertyConfigurator::configure("ta_rv2db_logconfig.cfg");
	logger = Logger::getLogger("Trace_rv2db");
	LOG4CXX_TRACE(logger, "run rv2db\n");
	InitializeEngine(lib_name);
	void * state = NULL;             // pointer to EyeFace state
	if (!(state = fcnEfInitEyeFace(EYEFACE_DIR, EYEFACE_DIR, EYEFACE_DIR, CONFIG_INI)))
	{
		fprintf(stderr, "ERROR 103: efInitEyeFace() failed.\n");
		LOG4CXX_TRACE(logger, "ERROR 103: efInitEyeFace() failed.\n");
		freeDll();
		return 103;
	}
	fcnEfShutdownEyeFace(state);
	fcnEfClearEyeFaceState(state);
	fcnEfFreeEyeFaceState(state);
	ifstream is("./rvdb_config.xml");
	SeT t = readset(is);
	is.close();
	if (t.size()>0)
	{
		strcpy(sql_server, t.at(0).sql_server.c_str());
		LOG4CXX_TRACE(logger, sql_server);
		strcpy(sql_user, t.at(0).sql_user.c_str());
		LOG4CXX_TRACE(logger, sql_user);
		strcpy(sql_passwd, t.at(0).sql_passwd.c_str());
		LOG4CXX_TRACE(logger, sql_passwd);
		strcpy(sql_db, t.at(0).sql_db.c_str());
		LOG4CXX_TRACE(logger, sql_db);
		strcpy(qpid_server, t.at(0).qpid_server.c_str());
		LOG4CXX_TRACE(logger, qpid_server);
		strcpy(qpid_address, t.at(0).qpid_address.c_str());
		LOG4CXX_TRACE(logger, qpid_address);
		DAYS = atoi(t.at(0).days.c_str());
		LOG4CXX_TRACE(logger, "compare days " << DAYS);
		FV = (float)atoi(t.at(0).fv.c_str()) / 100;
		LOG4CXX_TRACE(logger, "FV " << FV);
	}
	else
	{
		LOG4CXX_TRACE(logger, _T("rv2db no config file"));
		return 0;
	}
	mkdir(GetDateString().c_str(), 0775);
	LOG4CXX_TRACE(logger, GetDateString());
	while (1)
	{
		main2(argc, argv);
	}
}
