#include "stdafx.h"
#include "MLUtils.h"

namespace PticaGovorun {

EMQuick::EMQuick(int nclusters, int covMatType) : cv::EM(nclusters, covMatType)
{
}

EMQuick::EMQuick(int nclusters, int covMatType, const cv::TermCriteria& termCrit) : cv::EM(nclusters, covMatType, termCrit)
{
}

cv::Vec2d EMQuick::predict2(cv::InputArray _sample, const cv::Mat& meansPar, const std::vector<cv::Mat>& invCovsEigenValuesPar, const cv::Mat& logWeightDivDetPar, cv::Mat& cacheL)
{
	cv::Mat sample = _sample.getMat();
	//CV_Assert(isTrained());

	CV_Assert(!sample.empty());
	CV_Assert(sample.type() == CV_64FC1);
	CV_Assert(sample.rows == 1);

	return computeProbabilitiesInplace(sample, meansPar, invCovsEigenValuesPar, logWeightDivDetPar, cacheL);
}

cv::Vec2d EMQuick::computeProbabilitiesInplace(cv::Mat& sample, const cv::Mat& meansPar, const std::vector<cv::Mat>& invCovsEigenValuesPar, const cv::Mat& logWeightDivDetPar, cv::Mat& cacheL)
{
	// L_ik = log(weight_k) - 0.5 * log(|det(cov_k)|) - 0.5 *(x_i - mean_k)' cov_k^(-1) (x_i - mean_k)]
	// q = arg(max_k(L_ik))
	// probs_ik = exp(L_ik - L_iq) / (1 + sum_j!=q (exp(L_ij - L_iq))
	// see Alex Smola's blog http://blog.smola.org/page/2 for
	// details on the log-sum-exp trick

	const cv::Mat& means = meansPar;
	const std::vector<cv::Mat>& invCovsEigenValues = invCovsEigenValuesPar;
	const cv::Mat& logWeightDivDet = logWeightDivDetPar;
	int nclusters = means.rows;

	CV_Assert(!means.empty());
	CV_Assert(sample.type() == CV_64FC1);
	CV_Assert(sample.rows == 1);
	CV_Assert(sample.cols == means.cols);

	int dim = sample.cols;

	//Mat L(1, nclusters, CV_64FC1);
	//Mat& L = cacheL;
	cv::Mat& L = cacheL;

	int label = 0;
	for (int clusterIndex = 0; clusterIndex < nclusters; clusterIndex++)
	{
		//const auto& clusterMeanRow = means.row(clusterIndex);
		//sample -= clusterMeanRow;
		for (int i = 0; i < sample.cols; ++i)
			//sample.at<double>(0, i) -= clusterMeanRow.at<double>(0, i);
			sample.at<double>(0, i) -= means.at<double>(clusterIndex, i);

		cv::Mat& centeredSample = sample;

		//Mat rotatedCenteredSample = covMatType != EM::COV_MAT_GENERIC ?
		//centeredSample : centeredSample * covsRotateMats[clusterIndex];
		cv::Mat& rotatedCenteredSample = centeredSample;

		double Lval = 0;
		for (int di = 0; di < dim; di++)
		{
			//double w = invCovsEigenValues[clusterIndex].at<double>(covMatType != EM::COV_MAT_SPHERICAL ? di : 0);
			double w = invCovsEigenValues[clusterIndex].at<double>(0);
			double val = rotatedCenteredSample.at<double>(di);
			Lval += w * val * val;
		}
		CV_DbgAssert(!logWeightDivDet.empty());
		L.at<double>(clusterIndex) = logWeightDivDet.at<double>(clusterIndex) -0.5 * Lval;

		if (L.at<double>(clusterIndex) > L.at<double>(label))
			label = clusterIndex;

		//sample += clusterMeanRow;
		for (int i = 0; i < sample.cols; ++i)
			//sample.at<double>(0, i) += clusterMeanRow.at<double>(0, i);
			sample.at<double>(0, i) += means.at<double>(clusterIndex, i);
	}

	double maxLVal = L.at<double>(label);
	cv::Mat expL_Lmax = L; // exp(L_ij - L_iq)
	for (int i = 0; i < nclusters; i++)
		expL_Lmax.at<double>(i) = std::exp(L.at<double>(i) -maxLVal);
	double expDiffSum = sum(expL_Lmax)[0]; // sum_j(exp(L_ij - L_iq))

	cv::Vec2d res;
	res[0] = std::log(expDiffSum) + maxLVal - 0.5 * dim * CV_LOG2PI;
	res[1] = label;

	return res;
}

}