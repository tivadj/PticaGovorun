% Computes recognition rate, taking into account topN best results.
% For each sample check whether the pattern with max probability match the
% correct label, then descends by checking next (smaller) max value, etc.
% Continues topN times.
% predictedProbs [PatCount SamplesCount]
% correctLabels [1 SamplesCount]
% topN = specifies how many labels to asses when comparing the match of the
% labels
function recogRate = accuracyTopN(predictedProbs, correctLabels, topN)
    [PatCount SamplesCount] = size(predictedProbs);
    
    assert(size(correctLabels,1) == 1)
    assert(size(correctLabels,2) == SamplesCount)
    assert(topN >= 1 && topN <= PatCount)
    
    classifMask = zeros(1,SamplesCount,'uint8');
    for i=1:topN
        [maxVals,maxInds]=max(predictedProbs,[],1);
        
        classifMaskOne = (maxInds == correctLabels);
        classifMask = classifMask | classifMaskOne;
        
        % exclude indices of max values from further search
        maxIndsUni = sub2ind(size(predictedProbs), maxInds, 1:SamplesCount);
        predictedProbs(maxIndsUni) = -1;
    end
    
    recogRate = sum(classifMask) / SamplesCount;

end