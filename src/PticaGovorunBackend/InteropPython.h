#pragma once
#include <string>
#include "PticaGovorunCore.h" // PG_EXPORTS

namespace PticaGovorun
{
	PG_EXPORTS int phoneNameToPhoneId(const std::string& phoneName);

	// Also used for drawing phones grid.
	PG_EXPORTS void phoneIdToByPhoneName(int phoneId, std::string& phoneName);

	extern "C" PG_EXPORTS int phoneMonoCount();
	
	// The MFCC functionality is implemented based on Julius recognizer.
#ifdef PG_HAS_JULIUS
	// Creates phone -> MFCC features map.
	extern "C" PG_EXPORTS int createPhoneToMfccFeaturesMap();
	
	// Loads data into phone -> MFCC features map.
	extern "C" PG_EXPORTS bool loadPhoneToMfccFeaturesMap(int phoneToMfccFeaturesMapId, const wchar_t* folderOrWavFilesPath, int frameSize, int frameShift, int mfccVecLen);

	// Converts phone -> MFCC features map into table format.
	extern "C" PG_EXPORTS int reshapeAsTable_GetRowsCount(int phoneToMfccFeaturesMapId, int mfccVecLen);
	extern "C" PG_EXPORTS bool reshapeAsTable(int phoneToMfccFeaturesMapId, int mfccVecLen, int tableRowsCount, float* outFeaturesByFrame, int* outPhoneIds);
	
#if PG_HAS_OPENCV
	// Trains one cv::EM object per phone.
	extern "C" PG_EXPORTS bool trainMonophoneClassifier(int phoneToMfccFeaturesMapId, int mfccVecLen, int numClusters, int* classifierId);
#endif

	// Simulates classifier on the given feature vector.
	extern "C" PG_EXPORTS bool evaluateMonophoneClassifier(int classifierId, const float* features, int featuresCountPerFrame, int framesCount, int* phoneIdArray, float* logProbArray);
#endif
}