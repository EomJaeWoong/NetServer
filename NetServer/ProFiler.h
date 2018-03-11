#ifndef __PROFILER__H__
#define __PROFILER__H__

/*-------------------------------------------------------------------*/
// 프로파일러 기본 설정
//
// eMAX_THREAD_NAME		: 쓰레드 이름 최대 길이
// eMAX_SAMPLE			: 한 쓰레드의 최대 샘플 갯수
// eMAX_SAMPLE_THREAD	: 쓰레드의 최대 갯수
/*-------------------------------------------------------------------*/
enum e_CONFIG_PROFILER
{
	eMAX_SAMPLE_NAME	= 64,
	eMAX_SAMPLE			= 100,
	eMAX_THREAD_SAMPlE	= 50
};

///////////////////////////////////////////////////////////////////////
// 정보 수집할 단위 샘플
///////////////////////////////////////////////////////////////////////
struct st_SAMPLE
{
	bool				bUseFlag;

	WCHAR				wName[eMAX_SAMPLE_NAME];

	LARGE_INTEGER		liStartTime;

	double				dTotalSampleTime;

	double				dMaxTime[2];
	double				dMinTime[2];

	unsigned long		lCallCount;
};

///////////////////////////////////////////////////////////////////////
// 정보를 수집할 쓰레드 정보
//
// lThreadID : 쓰레드 ID
// pSample	 : 샘플 배열 포인터
///////////////////////////////////////////////////////////////////////
struct st_THREAD_SAMPLE
{
	DWORD				lThreadID;
	st_SAMPLE			*pSample;
};



///////////////////////////////////////////////////////////////////////
// 프로파일러 초기화
///////////////////////////////////////////////////////////////////////
bool					ProfileInit();

///////////////////////////////////////////////////////////////////////
// 프로파일링 시작
///////////////////////////////////////////////////////////////////////
bool					ProfileBegin(WCHAR *pwSampleName);

///////////////////////////////////////////////////////////////////////
// 프로파일링 끝
///////////////////////////////////////////////////////////////////////
bool					ProfileEnd(WCHAR *pwSampleName);



///////////////////////////////////////////////////////////////////////
// 프로파일링한 데이터 저장
///////////////////////////////////////////////////////////////////////
bool					SaveProfile();



///////////////////////////////////////////////////////////////////////
// 샘플 얻기
///////////////////////////////////////////////////////////////////////
bool					GetSample(WCHAR *pwSampleName, st_SAMPLE **pOutSample);

#endif



#ifdef PROFILE_CHECK
	#define PRO_BEGIN(X)	ProfileBegin(X)
	#define PRO_END(X)		ProfileEnd(X)
#else
	#define PRO_BEGIN(X)
	#define PRO_END(X)
#endif