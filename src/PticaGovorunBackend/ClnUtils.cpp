#include "stdafx.h"
#include "ClnUtils.h"

namespace PticaGovorun
{

// Divdes [min;max] interval into 'numPoints' points. If numPoints=2 the result is two points [min,max].
// The routine is similar to Matlab 'linspace' function.
void linearSpace(float min, float max, int numPoints, wv::slice<float> points)
{
	PG_Assert(numPoints >= 2);
	PG_Assert(points.size() >= numPoints && "Space for points must be allocated");

	// to prevent float round offs, set the first and the last points to expected min and max vlues
	points[0] = min;
	points[numPoints - 1] = max;

	// assign internal points
	for (int i = 1; i < numPoints - 1; ++i)
	{
		points[i] = min + (max - min) * i / (numPoints - 1);
	}

}

}