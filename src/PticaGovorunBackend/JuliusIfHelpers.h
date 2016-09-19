#pragma once
// Recog*
//#include <julius/recog.h>
#include <julius/julius.h>

#if defined JULIUSIFAPI_EXPORTS
#define JULIUSIF_EXPORTS __declspec(dllexport)
#else
#define JULIUSIF_EXPORTS __declspec(dllimport)
#endif

typedef void(*myRecogOutputTextPass1Delegate)(const char* woutput);
typedef void(*myRecogOutputWordSeqPass1Delegate)(const char* wname);
typedef void(*myRecogOutputPhonemeSeqPass1_PushPhonemeSeqDelegate)();
typedef void(*myRecogOutputPhonemeSeqPass1_AddPhonemeDelegate)(const char* wseq);
typedef void(*myRecogStatusDelegate)(int recogStatus);
typedef void(*myRecogOutputWordSeqPass2Delegate)(const char* wname);
typedef void(*myRecogOutputPhonemeSeq1Delegate)(const char* name, float avgScore, int begFrame, int endFrameIncl);

#ifdef __cplusplus
extern "C"
{
#endif
	JULIUSIF_EXPORTS void setOnRecogStatusChanged(myRecogStatusDelegate value);
	JULIUSIF_EXPORTS void setOnRecogOutputTextPass1(myRecogOutputTextPass1Delegate value);
	JULIUSIF_EXPORTS void setOnRecogOutputWordSeqPass1(myRecogOutputWordSeqPass1Delegate value);
	JULIUSIF_EXPORTS void setOnRecogOutputPhonemeSeqPass1_PushPhonemeSeq(myRecogOutputPhonemeSeqPass1_PushPhonemeSeqDelegate value);
	JULIUSIF_EXPORTS void setOnRecogOutputPhonemeSeqPass1_AddPhoneme(myRecogOutputPhonemeSeqPass1_AddPhonemeDelegate value);
	JULIUSIF_EXPORTS void setOnRecogOutputWordSeqPass2(myRecogOutputWordSeqPass2Delegate value);
	JULIUSIF_EXPORTS void setOnRecogOutputPhonemeSeq1(myRecogOutputPhonemeSeq1Delegate value);
	JULIUSIF_EXPORTS Recog* myGlobalRecog();
	JULIUSIF_EXPORTS void my_main_recognition_stream_loop();
#ifdef __cplusplus
};
#endif
