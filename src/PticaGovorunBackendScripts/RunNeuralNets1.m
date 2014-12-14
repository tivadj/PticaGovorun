table1=csvread('speech_MFCC1.txt');
feats = table1(:,1:size(table1,2)-1);
phoneIds = table1(:,size(table1,2));

fprintf('phones tally:\n')
[(1:max(phoneIds))' histc(phoneIds(:),1:max(phoneIds))]

%% Try feedforwardnet
% 'trainlm' =bad, Levenberg-Marquardt backpropagation, (default), 
% 'traingd' = bad, Gradient descent backpropagation
% 'traingda' =bad, Gradient descent with adaptive learning rate backpropagation
% 'traingdx', =bad, Gradient descent with momentum and adaptive learning rate backpropagation
% 'traingdm', =bad, Gradient descent with momentum backpropagation
net = feedforwardnet([39 39],'trainlm');
%net1 = configure(net1, feats, phoneIds);
%view(net1)
[net,tr] = train(net, feats', phoneIds')
phoneIdsFloatSim = net(feats');
perf = perform(net, phoneIdsFloatSim, phoneIds')
%mse(net,  phoneIdsFloatSim, phoneIds')
phoneIdsSim = round(phoneIdsFloatSim);

recogRate = sum(phoneIdsSim == phoneIds') / length(phoneIds)
%mae = sum(abs(phoneIdsSim - phoneIds')) / length(phoneIds)
%mse = sqrt(sum((phoneIdsFloatSim - phoneIds').^2) / length(phoneIds))


%% convert expected output to patternnet format 
% (NumClasses,NumSamples)
phCount = max(phoneIds);
% phonePatTarget = zeros(phCount, length(phoneIds));
% for phId = 1:phCount
%     phonePatTarget(phId, phoneIds == phId) = 1;
% end
phonePatTarget=full(ind2vec(phoneIds',phCount)); % [phCount,length(phoneIds)]
%% Try ::patternnet (recogRate<85%, layers=[3221], trainTime=1min)
% 'trainscg', Scaled conjugate gradient backpropagation
net = patternnet([390]);
[net,tr] = train(net, feats', phonePatTarget);
phoneIdsFloatSim = net(feats');
perf = crossentropy(net, phonePatTarget, phoneIdsFloatSim)

[C,phoneIdsSim]=max(phoneIdsFloatSim,[],1);
%phoneIdsSim = vec2ind(phoneIdsFloatSim);
recogRate = sum(phoneIdsSim == phoneIds') / length(phoneIds)
%% Use random search for ::patternnet
LayersMax = 1;
LayerSizeMax = 16000;

bestRecogRate=0;
bestHiddenLayersConfig=[];
for it=1:1000
    % choose random network configuration
    hiddenLayerCount = randi([1 LayersMax]);
    hiddenLayersConfig = randi([1 LayerSizeMax], 1, hiddenLayerCount, 'double');

    % train
    net = patternnet(hiddenLayersConfig);
    [net,tr] = train(net, feats', phonePatTarget);
    phoneIdsFloatSim = net(feats');

    % check results
    [C,phoneIdsSim]=max(phoneIdsFloatSim,[],1);
    recogRate = sum(phoneIdsSim == phoneIds') / length(phoneIds);
    if recogRate > bestRecogRate
        bestRecogRate = recogRate;
        bestHiddenLayersConfig = hiddenLayersConfig;
        display({bestRecogRate,bestHiddenLayersConfig});
    end
    fprintf('recogRate=%f net=%s t=%f\n', recogRate, mat2str(hiddenLayersConfig), max(tr.time));
end

%% Try ::cascadeforwardnet (recogRate<76%, layers=[390], trainSpeed 5min, mem 6Gb)
net = cascadeforwardnet([390]);
[net,tr] = train(net, feats', phoneIds');
phoneIdsFloatSim = net(feats');
phoneIdsSim = round(phoneIdsFloatSim);

recogRate = sum(phoneIdsSim == phoneIds') / length(phoneIds)

%% Try ::selforgmap (recogRate=15%)
phCount = max(phoneIds)
net = selforgmap([2 3]);
net = train(net,feats');
phoneIdsPat = net(feats');
phoneIdsSim = vec2ind(phoneIdsPat);
recogRate = sum(phoneIdsSim == phoneIds') / length(phoneIds)