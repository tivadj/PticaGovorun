% Simulates network on given features.
% feats[FeatsCount, SamplesCount, double]
function predictedProbs = simulateNet(net, feats)
    display(net)
    display(feats)
    predictedProbs = net(feats);
end