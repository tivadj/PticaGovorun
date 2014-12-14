#include "stdafx.h"
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include "InteropPython.h"
#include "SpeechProcessing.h"
#include "JuliusToolNativeWrapper.h"

namespace PticaGovorun {
	int globalResourceIdCounter_ = 100;

	// required to get MFCC features
	std::unique_ptr<JuliusToolWrapper> recognizer_;

	// features
	std::map<int, std::map<std::string, std::vector<float>>> globalIdToPhoneNameToFeaturesVector_;

	// classifiers
	std::map<int, std::map<std::string, std::unique_ptr<PticaGovorun::EMQuick>>> globalPhoneNameToEMObj_;

	std::vector<const char*> phoneIdToPhoneName_ = { "?", "a", "e", "y", "i", "o", "u" };
	std::map<std::string, int> phoneNameToPhoneId_;

	bool globalInitialized_ = false;
	bool initialize()
	{
		if (globalInitialized_)
			return true;

		int phoneId = 0;
		for (const char* phoneName : phoneIdToPhoneName_)
			phoneNameToPhoneId_[phoneName] = phoneId++;

		//
		RecognizerSettings rs;
		if (!initRecognizerConfiguration("shrekky", rs))
		{
			std::wcerr <<"Can't find speech recognizer configuration";
			return false;
		}

		QTextCodec* pTextCodec = QTextCodec::codecForName(PGEncodingStr);
		auto textCodec = std::unique_ptr<QTextCodec, NoDeleteFunctor<QTextCodec>>(pTextCodec);

		std::tuple<bool, std::string, std::unique_ptr<JuliusToolWrapper>> newRecogOp = createJuliusRecognizer(rs, std::move(textCodec));
		if (!std::get<0>(newRecogOp))
		{
			auto err = std::get<1>(newRecogOp);
			std::wcerr << err.c_str();
			return false;
		}

		std::wcout << "Recognizer is initialized" <<std::endl;

		recognizer_ = std::move(std::get<2>(newRecogOp));
		globalInitialized_ = true;
		return true;
	}

	int createPhoneToMfccFeaturesMap()
	{
		bool initOp = initialize();
		if (!initOp)
			return -1;

		std::map<std::string, std::vector<float>> newMap;

		int newId = ++globalResourceIdCounter_;
		globalIdToPhoneNameToFeaturesVector_.insert(std::make_pair(newId, std::move(newMap)));
		
		return newId;
	}

	bool loadPhoneToMfccFeaturesMap(int phoneToMfccFeaturesMapId, const wchar_t* folderOrWavFilesPath, int frameSize, int frameShift, int mfccVecLen)
	{
		auto mapIt = globalIdToPhoneNameToFeaturesVector_.find(phoneToMfccFeaturesMapId);
		if (mapIt == std::end(globalIdToPhoneNameToFeaturesVector_))
		{
			std::wcerr << "Error: map with ID=" << phoneToMfccFeaturesMapId << " was not initialized" << std::endl;
			return false;
		}

		std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector = mapIt->second;
		QFileInfo wavFolder(QString::fromWCharArray(folderOrWavFilesPath));
		auto featsOp = collectMfccFeatures(wavFolder, frameSize, frameShift, mfccVecLen, phoneNameToFeaturesVector);
		if (!std::get<0>(featsOp))
		{
			std::wcerr << std::get<1>(featsOp) << std::endl;
			return false;
		}

		return true;
	}


	int reshapeAsTable_GetRowsCount(int phoneToMfccFeaturesMapId, int mfccVecLen)
	{
		auto mapIt = globalIdToPhoneNameToFeaturesVector_.find(phoneToMfccFeaturesMapId);
		if (mapIt == std::end(globalIdToPhoneNameToFeaturesVector_))
		{
			std::wcerr << "Error: map with ID=" << phoneToMfccFeaturesMapId << " was not initialized" << std::endl;
			return -1;
		}

		int totalFramesCount = 0;

		const std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector = mapIt->second;
		for (const auto& phoneNameToFeatsPair : phoneNameToFeaturesVector)
		{
			const std::vector<float>& feats = phoneNameToFeatsPair.second;
			int framesCount = featuresFramesCount(feats.size(), mfccVecLen);
			totalFramesCount += framesCount;
		}
		return totalFramesCount;
	}

	bool reshapeAsTable(int phoneToMfccFeaturesMapId, int mfccVecLen, int tableRowsCount, float* outFeaturesByFrame, int* outPhoneIds)
	{
		auto mapIt = globalIdToPhoneNameToFeaturesVector_.find(phoneToMfccFeaturesMapId);
		if (mapIt == std::end(globalIdToPhoneNameToFeaturesVector_))
		{
			std::wcerr << "Error: map with ID=" << phoneToMfccFeaturesMapId << " was not initialized" << std::endl;
			return false;
		}

		int totalFramesCount = 0;

		float* pOutFeatures = outFeaturesByFrame;
		int* pOutPhoneIds = outPhoneIds;

		const std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector = mapIt->second;
		for (const auto& phoneNameToFeatsPair : phoneNameToFeaturesVector)
		{
			const std::vector<float>& featuresPerPhone = phoneNameToFeatsPair.second;
			int framesCountPerPhone = featuresFramesCount(featuresPerPhone.size(), mfccVecLen);
			totalFramesCount += framesCountPerPhone;

			if (totalFramesCount > tableRowsCount)
			{
				std::wcerr << "Error: Not enough space is allocated for output arrays" << std::endl;
				return false;
			}

			// write features data

			std::copy(std::begin(featuresPerPhone), std::end(featuresPerPhone), pOutFeatures);
			pOutFeatures += featuresPerPhone.size();

			// write phone ids

			const std::string& phoneName = phoneNameToFeatsPair.first;

			int phoneId = phoneNameToPhoneId_[phoneName];

			std::generate_n(pOutPhoneIds, framesCountPerPhone, [phoneId]() -> int { return phoneId; });
			pOutPhoneIds += framesCountPerPhone;
		}

		if (totalFramesCount != tableRowsCount)
		{
			std::wcerr << "Error: Requested not all features" << std::endl;
			return false;
		}

		return true;
	}

	bool trainMonophoneClassifier(int phoneToMfccFeaturesMapId, int mfccVecLen, int numClusters, int* classifierId)
	{
		auto mapIt = globalIdToPhoneNameToFeaturesVector_.find(phoneToMfccFeaturesMapId);
		if (mapIt == std::end(globalIdToPhoneNameToFeaturesVector_))
		{
			std::wcerr << "Error: features map with ID=" << phoneToMfccFeaturesMapId << " was not initialized" << std::endl;
			return false;
		}

		if (classifierId == nullptr)
		{
			std::wcerr << "Error: classifierId == null" << std::endl;
			return false;
		}

		std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector = mapIt->second;
		std::map<std::string, std::unique_ptr<PticaGovorun::EMQuick>> phoneNameToEMObj;
		auto trainOp = trainMonophoneClassifier(phoneNameToFeaturesVector, mfccVecLen, numClusters, phoneNameToEMObj);
		if (!std::get<0>(trainOp))
		{
			std::wcerr <<"Error: " << std::get<1>(trainOp);
			return false;
		}

		int newId = ++globalResourceIdCounter_;
		globalPhoneNameToEMObj_.insert(std::make_pair(newId, std::move(phoneNameToEMObj)));
		*classifierId = newId;

		return true;
	}

	bool evaluateMonophoneClassifier(int classifierId, const float* features, int featuresCountPerFrame, int framesCount, int* phoneIdArray, float* logProbArray)
	{
		auto mapIt = globalPhoneNameToEMObj_.find(classifierId);
		if (mapIt == std::end(globalPhoneNameToEMObj_))
		{
			std::wcerr << "Error: the classifier with ID=" << classifierId << " was not initialized" << std::endl;
			return false;
		}

		if (features == nullptr)
		{
			std::wcerr << "Error: features == null" << std::endl;
			return false;
		}

		std::map<std::string, std::unique_ptr<PticaGovorun::EMQuick>>& phoneNameToEMObj = mapIt->second;

		// take number of clusters from any trained classifer
		// assumes, there exist one classfier and it was trained
		int numClusters = std::begin(phoneNameToEMObj)->second->getNClusters();
		std::wcout << "numClusters=" << numClusters <<std::endl;

		//
		std::vector<double> featuresDouble(featuresCountPerFrame);

		cv::Mat cacheL;
		cacheL.create(1, numClusters, CV_64FC1);

		for (int frameInd = 0; frameInd < framesCount; ++frameInd)
		{
			double maxLogProb = std::numeric_limits<double>::lowest();
			int bestPhoneId = -1;

			for (const auto& phoneEMPair : phoneNameToEMObj)
			{
				const std::string& phoneName = phoneEMPair.first;
				int phoneId = phoneNameToPhoneId_[phoneName];

				PticaGovorun::EMQuick& em = *phoneEMPair.second.get();

				// convert float features to double
				featuresDouble.assign(features + frameInd * featuresCountPerFrame, features + (frameInd + 1) * featuresCountPerFrame);
				cv::Mat oneFeatsVector(1, featuresCountPerFrame, CV_64FC1, featuresDouble.data());

				//
				cv::Vec2d resP2 = PticaGovorun::EMQuick::predict2(oneFeatsVector, em.getMeans(), em.getInvCovsEigenValuesPar(), em.getLogWeightDivDetPar(), cacheL);
				double logProb = resP2[0];
				if (logProb > maxLogProb)
				{
					maxLogProb = logProb;
					bestPhoneId = phoneId;
				}
			}

			if (phoneIdArray != nullptr)
				phoneIdArray[frameInd] = bestPhoneId;
			if (logProbArray != nullptr)
				logProbArray[frameInd] = (float)maxLogProb;
		}

		return true;
	}
}