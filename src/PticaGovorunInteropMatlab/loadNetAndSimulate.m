% Simulates network on given features.
% feats[FeatsCount, SamplesCount, float_or_double]
% As there is a problem in transfering objects across Matlab/Native
% boundary, we load and simulate the network in one step.
function predictedProbs = loadNetAndSimulate(netPath, feats)
    % this is a special pragma for MATLAB Compiler
    % used to declare "network" class as dependency in deployed mode
    %#function network
    
    %net = patternnet();
    
    % load pre-trained network
    netStruct = load(netPath, 'net');
    net = netStruct.net;
    display(net);
    
    %predictedProbs = sim(net, feats);
    predictedProbs = net(feats);
    display(predictedProbs);
end