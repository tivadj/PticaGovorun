#include "PhoneAlignment.h"
#include <limits>
#include "assertImpl.h"

using namespace std;

namespace PticaGovorun {

PhoneAlignment::PhoneAlignment(size_t statesCount, size_t framesCount, std::function<double(size_t,size_t)> emitFun)
    :statesCount_(statesCount),
    framesCount_(framesCount),
    emitFun_(emitFun)
{
    if (statesCount_ < 0)
        throw std::exception("statesCount must be >= 1");
    if (framesCount < statesCount_)
        throw std::exception("number of frames must be >= number of states");

    // assert: all emitting probabilities >= 0
}

PhoneAlignment::~PhoneAlignment(void)
{
}

// Each pair contains Inclusive coordinates of the states. 
// Consequent segments are adjacent ([x1,x2],[x3,x4]), x3=x2+1.
void PhoneAlignment::compute(std::vector<std::tuple<size_t,size_t>>& resultAlignedStates)
{
    statesTrellis_.resize(statesCount_ * framesCount_);

    // init first column - states in the first frame

    if (framesCount_ > 0) 
    {
        state(0,0) = emitFunSafe(0,0);

        for (size_t stateInd=1; stateInd < statesCount_; ++stateInd)
            state(stateInd,0) = ProbabilityType::zeroValue();
    }

    // init first row 

    for (size_t timeInd=1; timeInd < framesCount_; ++timeInd)
        state(0,timeInd) = ProbabilityType::joint(emitFunSafe(0,timeInd), state(0,timeInd-1));

    // fill all states

    for (size_t stateInd=1; stateInd < statesCount_; ++stateInd)
    {
        for (size_t timeInd=1; timeInd < framesCount_; ++timeInd)
        {
            auto prevBelow = state(stateInd-1,timeInd-1);
            auto prevSame = state(stateInd,timeInd-1);
            auto max1 = ProbabilityType::maxProb(prevBelow, prevSame);

            auto emitProb = emitFunSafe(stateInd,timeInd);
            auto stateProb = ProbabilityType::joint(emitProb, max1);

            state(stateInd,timeInd) = stateProb;
        }
    }

    resultAlignedStates.clear();
    resultAlignedStates.reserve(statesCount_);
    populateOptimalAlignment(resultAlignedStates);

	PG_DbgAssert2(resultAlignedStates.size() == statesCount_, "All frames must be consequently assigned to the states");
}

void PhoneAlignment::populateOptimalAlignment(std::vector<std::tuple<size_t,size_t>>& resultAlignedStates)
{
	PG_DbgAssert(resultAlignedStates.empty());

    if (framesCount_ < 1 || statesCount_ < 1) // no data to process
        return;

    // do backward track

    size_t timeInd = framesCount_-1; // last frame
    size_t stateInd = statesCount_-1; // last state
    size_t segmentEnd = timeInd;
    while (true)
    {
        if (stateInd == 0 && timeInd == 0) // traced to the beginning
        {
            auto seg = make_tuple(timeInd, segmentEnd);
            resultAlignedStates.push_back(seg);

            break;
        }

        if (timeInd == 0)
			PG_DbgAssert2(stateInd == 0, "In the frame == 0 only state == 0 is possible (to end up in the beginning cell)");

        auto prob = state(stateInd, timeInd-1); // step from the same state

        if (stateInd > 0) // step from different state
        {
            auto probBelow = state(stateInd-1, timeInd-1);
            if (ProbabilityType::opLess(prob, probBelow))
            {
                // move to the next state
                // current segment is built
                auto seg = make_tuple(timeInd, segmentEnd);
                resultAlignedStates.push_back(seg);

                segmentEnd = timeInd - 1; // move (left) to the next segment
                --stateInd;
            }
        }

        --timeInd;
    }

    reverse(begin(resultAlignedStates), end(resultAlignedStates));
}

void PhoneAlignment::populateStateDistributions(const std::vector<std::tuple<size_t,size_t>>& statesAlignment, size_t tailSize,
    std::vector<PhoneStateDistribution>& resultStateProbs)
{
    for(size_t stateIndex = 0; stateIndex < statesAlignment.size(); ++stateIndex)
    {
		const std::tuple<size_t, size_t>& seg = statesAlignment[stateIndex];

        ptrdiff_t left = std::get<0>(seg) - tailSize;
        ptrdiff_t right = std::get<1>(seg) + tailSize;

        PhoneStateDistribution stateDistrib;
        stateDistrib.OffsetFrameIndex = -1;

        for (ptrdiff_t frameIndex=left; frameIndex <= right; ++frameIndex)
        {
            if (frameIndex < 0 || frameIndex >= framesCount_)
                continue;

            auto logProb = emitFunSafe(stateIndex, frameIndex);

            // skip impossible probabilities
            if (ProbabilityType::isZero(logProb))
                continue;

            if (stateDistrib.OffsetFrameIndex == -1)
            {
				PG_DbgAssert2(frameIndex >= 0, "valid index must be found");
                stateDistrib.OffsetFrameIndex = (size_t)frameIndex;
            }

            stateDistrib.LogProbs.push_back(logProb);
        }

        resultStateProbs.push_back(stateDistrib);
    }
}

double PhoneAlignment::getAlignmentScore() const
{
    return state(statesCount_-1, framesCount_-1);
}

double PhoneAlignment::emitFunSafe(size_t stateIndex, size_t frameIndex) const
{
    auto prob = emitFun_(stateIndex, frameIndex);

    // check prob is not infinity
	PG_DbgAssert2(prob != std::numeric_limits<double>::lowest(), "Emit probability can't be infinity");
    return prob;
}

}