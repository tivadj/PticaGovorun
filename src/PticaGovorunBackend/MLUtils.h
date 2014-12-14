#pragma once

#include <opencv2\core.hpp> // cv::Mat
#include <opencv2\ml.hpp> // cv::EM
#include "PticaGovorunCore.h"

namespace PticaGovorun {

// TODO: this should have an internal usage
class PG_EXPORTS EMQuick : public cv::EM
{
public:
	EMQuick(int nclusters, int covMatType);
	EMQuick(int nclusters, int covMatType, const cv::TermCriteria& termCrit);
	EMQuick(const EMQuick&) = delete;
	static cv::Vec2d predict2(cv::InputArray _sample, const cv::Mat& meansPar, const std::vector<cv::Mat>& invCovsEigenValuesPar, const cv::Mat& logWeightDivDetPar, cv::Mat& cacheL);
	static cv::Vec2d computeProbabilitiesInplace(cv::Mat& sample, const cv::Mat& meansPar, const std::vector<cv::Mat>& invCovsEigenValuesPar, const cv::Mat& logWeightDivDetPar, cv::Mat& cacheL);

	int getNClusters() const
	{
		return this->nclusters;
	}
	const cv::Mat& getWeights() const
	{
		return this->weights;
	}
	const cv::Mat& getMeans() const
	{
		return this->means;
	}
	const std::vector<cv::Mat>& getCovs() const
	{
		return this->covs;
	}
	const std::vector<cv::Mat>& getInvCovsEigenValuesPar() const
	{
		return this->invCovsEigenValues;
	}
	const cv::Mat& getLogWeightDivDetPar() const
	{
		return this->logWeightDivDet;
	}
};

}