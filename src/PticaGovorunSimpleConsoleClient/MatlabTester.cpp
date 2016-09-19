#include <iostream>
#include <memory>
#include <array>
#if HAS_MATLAB
#include "matrix.h" // mxArray, mxCreateLogicalArray
#include "PticaGovorunInteropMatlab.h"
#endif

namespace MatlabTesterNS {
#if HAS_MATLAB
	void testClassifyWithMatlabNeuralNet()
	{
		std::cout << "Init Matlab runtime" << std::endl;

		auto errHandler = [](const char* s) { std::cerr << s; return 1; };
		auto printHandler = [](const char* s) { std::cout << s; return 1; };
		bool initMatlab = PticaGovorunInteropMatlabInitializeWithHandlers(errHandler, printHandler);
		if (!initMatlab)
		{
			std::cerr << "PWMatlabProxy initialization failed" << std::endl;
			return;
		}
		

		std::array<float, 39> featsArray = { 1.35582852363586, 0.118024349212646, -1.79822540283203, 0.492237865924835, -6.66390132904053, 3.09586453437805, 2.01883125305176, -2.46541428565979, -4.20148754119873, 5.01432132720947, 4.62824249267578, 0.585328042507172, 2.51930236816406, -0.189353853464127, 0.495635420084000, -0.956949055194855, -1.39910829067230, 1.21690428256989, -1.54861867427826, -0.674562096595764, -0.436688721179962, 1.26034593582153, -2.20753669738770, -1.14015078544617, 0.168150082230568, 0.0722404494881630, -0.0381436347961426, -0.255864292383194, 0.178694874048233, 0.685731530189514, 0.132833570241928, -0.440552890300751, -0.104280233383179, 0.392423182725906, -0.108158126473427, -0.0752929002046585, -0.138827919960022, -0.173502758145332, 0.0101397689431906 };
		mwArray featsMat(featsArray.size(), 1, mxSINGLE_CLASS);
		featsMat.SetData(featsArray.data(), featsArray.size());

		mwArray probsPerPatternMat;
		classifySpeechMfcc(1, probsPerPatternMat, featsMat);

		std::array<float, 6> yFloat;
		probsPerPatternMat.GetData(yFloat.data(), yFloat.size());

		//const char* netPath = R"path(C:\devb\PticaGovorunProj\srcrep\src\PticaGovorunInteropMatlab\netPatternnet282.mat)path";
		//mwArray netPathObj(netPath);
		//loadNetAndSimulate(1, predictedProbs, netPathObj, featsObj);

		PticaGovorunInteropMatlabTerminate();
	}

#endif

	void run()
	{
#if HAS_MATLAB
		testClassifyWithMatlabNeuralNet();
#else
		std::cerr << "Set HAS_MATLAB to call Matlab code" <<std::endl;
#endif
	}


} // PticaGovorun
