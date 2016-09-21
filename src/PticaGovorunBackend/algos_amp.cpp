#if WIN32 // or perhaps use MSVC
#include <amp.h>
#include <amp_math.h> // fast_math
#include <amp_graphics.h>

//#include <opencv2\ml.hpp> // cv::EM

//using namespace concurrency; // amp
//using namespace concurrency::graphics; // float_3

inline double computeGaussMixtureModel(int nclusters, int dim , const double* pMeans, const double* pInvCovsEigenValues, const double* pLogWeightDivDet, double* pSample, double* pCacheL)
restrict(cpu, amp) 
{
	// L_ik = log(weight_k) - 0.5 * log(|det(cov_k)|) - 0.5 *(x_i - mean_k)' cov_k^(-1) (x_i - mean_k)]
	// q = arg(max_k(L_ik))
	// probs_ik = exp(L_ik - L_iq) / (1 + sum_j!=q (exp(L_ij - L_iq))
	// see Alex Smola's blog http://blog.smola.org/page/2 for
	// details on the log-sum-exp trick

	//const Mat& means = meansPar;
	//const std::vector<Mat>& invCovsEigenValues = invCovsEigenValuesPar;
	//const Mat& logWeightDivDet = logWeightDivDetPar;
	//int nclusters = means.rows;

	//CV_Assert(!means.empty());
	//CV_Assert(sample.type() == CV_64FC1);
	//CV_Assert(sample.rows == 1);
	//CV_Assert(sample.cols == means.cols);

	//int dim = sample.cols;
	//const int dim = 3;

	//Mat L(1, nclusters, CV_64FC1);
	//Mat& L = cacheL;
	//Mat& L = cacheL;

	int label = 0;
	for (int clusterIndex = 0; clusterIndex < nclusters; clusterIndex++)
	{
		//const auto& clusterMeanRow = means.row(clusterIndex);
		//sample -= clusterMeanRow;
		for (int i = 0; i < dim; ++i)
			//sample.at<double>(0, i) -= clusterMeanRow.at<double>(0, i);
			//sample.at<double>(0, i) -= means.at<double>(clusterIndex, i);
			pSample[i] -= pMeans[clusterIndex * dim + i];

		//Mat& centeredSample = sample;

		//Mat rotatedCenteredSample = covMatType != EM::COV_MAT_GENERIC ?
		//centeredSample : centeredSample * covsRotateMats[clusterIndex];
		//Mat& rotatedCenteredSample = centeredSample;
		double* pRotatedCenteredSample = pSample;

		double Lval = 0;
		for (int di = 0; di < dim; di++)
		{
			//double w = invCovsEigenValues[clusterIndex].at<double>(covMatType != EM::COV_MAT_SPHERICAL ? di : 0);
			double w = pInvCovsEigenValues[clusterIndex];
			double val = pRotatedCenteredSample[di];
			Lval += w * val * val;
		}
		//CV_DbgAssert(!logWeightDivDet.empty());
		pCacheL[clusterIndex] = pLogWeightDivDet[clusterIndex] - 0.5 * Lval;

		if (pCacheL[clusterIndex] > pCacheL[label])
			label = clusterIndex;

		//sample += clusterMeanRow;
		for (int i = 0; i < dim; ++i)
			//sample.at<double>(0, i) += clusterMeanRow.at<double>(0, i);
			//sample.at<double>(0, i) += means.at<double>(clusterIndex, i);
			pSample[i] += pMeans[clusterIndex * 3 + i];
	}

	double maxLVal = pCacheL[label];

	//Mat expL_Lmax = L; // exp(L_ij - L_iq)
	//for (int i = 0; i < L.cols; i++)
	//	expL_Lmax.at<double>(i) = std::exp(pCacheL[i] - maxLVal);
	//double expDiffSum = sum(expL_Lmax)[0]; // sum_j(exp(L_ij - L_iq))

	double expDiffSum = 0;
	for (int i = 0; i < nclusters; i++)
	{
		//double expL = std::exp(pCacheL[i] - maxLVal);
		double expL = concurrency::precise_math::exp(pCacheL[i] - maxLVal);
		pCacheL[i] = expL;
		expDiffSum += expL;
	}

	//Vec2d res;
	//res[0] = std::log(expDiffSum) + maxLVal - 0.5 * dim * CV_LOG2PI;
	//res[1] = label;

	//double logProb = std::log(expDiffSum) + maxLVal - 0.5 * dim * CV_LOG2PI;

	// CV_LOG2PI
	/* log(2*PI) */
	const double PG_LOG2PI = 1.8378770664093454835606594728112;

	double logProb = concurrency::precise_math::log(expDiffSum) + maxLVal - 0.5 * dim * PG_LOG2PI;

	return logProb;
}

template <typename T>
inline T computeGaussMixtureModelGen(int nclusters, const T* pMeans, const T* pInvCovsEigenValues, const T* pLogWeightDivDet, T* pSample, T* pCacheL) restrict(cpu, amp)
{
	// L_ik = log(weight_k) - 0.5 * log(|det(cov_k)|) - 0.5 *(x_i - mean_k)' cov_k^(-1) (x_i - mean_k)]
	// q = arg(max_k(L_ik))
	// probs_ik = exp(L_ik - L_iq) / (1 + sum_j!=q (exp(L_ij - L_iq))
	// see Alex Smola's blog http://blog.smola.org/page/2 for
	// details on the log-sum-exp trick

	//const Mat& means = meansPar;
	//const std::vector<Mat>& invCovsEigenValues = invCovsEigenValuesPar;
	//const Mat& logWeightDivDet = logWeightDivDetPar;
	//int nclusters = means.rows;

	//CV_Assert(!means.empty());
	//CV_Assert(sample.type() == CV_64FC1);
	//CV_Assert(sample.rows == 1);
	//CV_Assert(sample.cols == means.cols);

	//int dim = sample.cols;
	const int dim = 3;

	//Mat L(1, nclusters, CV_64FC1);
	//Mat& L = cacheL;
	//Mat& L = cacheL;

	int label = 0;
	for (int clusterIndex = 0; clusterIndex < nclusters; clusterIndex++)
	{
		//const auto& clusterMeanRow = means.row(clusterIndex);
		//sample -= clusterMeanRow;
		for (int i = 0; i < dim; ++i)
			//sample.at<T>(0, i) -= clusterMeanRow.at<T>(0, i);
			//sample.at<T>(0, i) -= means.at<T>(clusterIndex, i);
			pSample[i] -= pMeans[clusterIndex * dim + i];

		//Mat& centeredSample = sample;

		//Mat rotatedCenteredSample = covMatType != EM::COV_MAT_GENERIC ?
		//centeredSample : centeredSample * covsRotateMats[clusterIndex];
		//Mat& rotatedCenteredSample = centeredSample;
		T* pRotatedCenteredSample = pSample;

		T Lval = 0;
		for (int di = 0; di < dim; di++)
		{
			//T w = invCovsEigenValues[clusterIndex].at<T>(covMatType != EM::COV_MAT_SPHERICAL ? di : 0);
			T w = pInvCovsEigenValues[clusterIndex];
			T val = pRotatedCenteredSample[di];
			Lval += w * val * val;
		}
		//CV_DbgAssert(!logWeightDivDet.empty());
		pCacheL[clusterIndex] = pLogWeightDivDet[clusterIndex] - 0.5 * Lval;

		if (pCacheL[clusterIndex] > pCacheL[label])
			label = clusterIndex;

		//sample += clusterMeanRow;
		for (int i = 0; i < dim; ++i)
			//sample.at<T>(0, i) += clusterMeanRow.at<T>(0, i);
			//sample.at<T>(0, i) += means.at<T>(clusterIndex, i);
			pSample[i] += pMeans[clusterIndex * 3 + i];
	}

	T maxLVal = pCacheL[label];

	//Mat expL_Lmax = L; // exp(L_ij - L_iq)
	//for (int i = 0; i < L.cols; i++)
	//	expL_Lmax.at<T>(i) = std::exp(pCacheL[i] - maxLVal);
	//T expDiffSum = sum(expL_Lmax)[0]; // sum_j(exp(L_ij - L_iq))

	T expDiffSum = 0;
	for (int i = 0; i < nclusters; i++)
	{
		//T expL = std::exp(pCacheL[i] - maxLVal);
		T expL = concurrency::fast_math::exp(pCacheL[i] - maxLVal);
		pCacheL[i] = expL;
		expDiffSum += expL;
	}

	//Vec2d res;
	//res[0] = std::log(expDiffSum) + maxLVal - 0.5 * dim * CV_LOG2PI;
	//res[1] = label;

	//T logProb = std::log(expDiffSum) + maxLVal - 0.5 * dim * CV_LOG2PI;
	T logProb = concurrency::fast_math::log(expDiffSum) + maxLVal - 0.5 * dim * CV_LOG2PI;

	return logProb;
}
#endif