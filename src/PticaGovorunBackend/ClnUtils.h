#pragma once

namespace PticaGovorun {

template <typename T>
struct NoDeleteFunctor
{
	void operator()(T* pObj) const
	{
		// no op
	}
};


// Performs the binary search to find the item with greatest associated item.FrameInd less than pivotFrameInd.
// The collection must be in ascending order.
// Numerical associated value is selected using frameIndSelector.
// T = long
// frameIndSelector :: *RandIt -> T  = the predicate to extract FrameInd from given item.
template <typename RandIt, typename ItemSelector>
auto binarySearch(const RandIt& begin, const RandIt& end, long frameIndValue, ItemSelector frameIndSelector) -> RandIt
{
	// the search finishes when (begin;end) are the two consecutive items; the result is the lesser value
	if (end - begin == 1)
		return begin;

	RandIt midIt = begin + (end - begin) / 2;
	long midFrameInd = frameIndSelector(*midIt);

	RandIt left;
	RandIt right;
	if (frameIndValue< midFrameInd)
	{
		// continue to search in the left half of the range
		left = begin;
		right = midIt;
	}
	else
	{
		// continue to search in the right half of the range
		left = midIt;
		right = end;
	}

	auto result = binarySearch(left, right, frameIndValue, frameIndSelector);
	return result;
}

}