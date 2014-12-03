#pragma once
#include <vector>
#include <functional>

namespace PticaGovorun {

template <typename T>
struct VanillaProbTraits
{
    static bool isZero(T probabilityValue)
    {
        return probabilityValue < logZero();
    }

    // all values below this value treated as prob = 0
    static T logZero()
    {
        return T(0.000001);
    }

    static T joint(T p1, T p2)
    {
        // simple probabilities
        return p1 * p2;
    }

    bool opLess(T p1, T p2)
    {
        // simple probabilities
        return p1 < p2;
    }
};

// Represents logarithms of probability, that is log(p).
template <typename T>
struct LogProbTraits
{
    static bool isZero(T probabilityValue)
    {
        return probabilityValue < zeroValue();
    }

    // all values below this value treated as prob = 0
    static T zeroValue()
    {
        return T(-1000000);
    }

    static T joint(T p1, T p2)
    {
        //if (isZero(p1) || isZero(p2))
        //    return zeroValue();

        // assume p1 and p2 are 
        auto result = p1 + p2;
        //if (isZero(result))
        //    result = zeroValue();
        
        return result;
    }

    static bool opLess(T p1, T p2)
    {
        // logarithmic
        //if (p1 == ZeroLogProb)
        //    return !(p2 == ZeroLogProb);

        //if (p2 == ZeroLogProb)
        //    return false;

        return p1 < p2;
    }

    static T maxProb(T p1, T p2)
    {
        bool isLess = opLess(p1, p2);
        return isLess ? p2 : p1;
    }
};


struct PhoneStateDistribution
{
    size_t OffsetFrameIndex;
    std::vector<double> LogProbs;
};

class PhoneAlignment
{
    size_t statesCount_;
    size_t framesCount_;
    std::vector<double> statesTrellis_; // column wise state trellis
    const std::function<double(size_t,size_t)> emitFun_;

    typedef LogProbTraits<double> ProbabilityType;
public:
    PhoneAlignment(size_t statesCount, size_t framesCount, std::function<double(size_t,size_t)> emitFun);
    ~PhoneAlignment(void);

    void compute(std::vector<std::tuple<size_t,size_t>>& resultAlignedStates);

    void populateStateDistributions(const std::vector<std::tuple<size_t,size_t>>& statesAlignment, size_t tailSize,
        std::vector<PhoneStateDistribution>& resultStateProbs);

    double getAlignmentScore() const;
private:
    void populateOptimalAlignment(std::vector<std::tuple<size_t,size_t>>& resultAlignedStates);

    double& state(size_t stateInd, size_t timeInd)
    { 
        size_t ind = timeInd * statesCount_ + stateInd;
        return statesTrellis_[ind];
    }

    double state(size_t stateInd, size_t timeInd) const
    { 
        size_t ind = timeInd * statesCount_ + stateInd;
        return statesTrellis_[ind];
    }

    double emitFunSafe(size_t stateIndex, size_t frameIndex) const;
};

}