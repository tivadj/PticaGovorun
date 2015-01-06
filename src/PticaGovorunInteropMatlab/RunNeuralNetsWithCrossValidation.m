table1=csvread('../../../data/speech_MFCC1_internal.txt');
Q=size(table1,1) % num samples
feats = table1(:,1:size(table1,2)-1)'; % [FeatCount,Q]
phoneIds = table1(:,size(table1,2))'; % [1,Q]
phoneIds = phoneIds+1; % 0-based to 1-based phone indices
fprintf('phones tally:\n')
phoneIdsUnique = min(phoneIds):max(phoneIds);
[phoneIdsUnique' histc(phoneIds(:),phoneIdsUnique)]

% phones count
phCount = length(phoneIdsUnique);

% convert expected output to patternnet format 
phonePatTarget=full(ind2vec(phoneIds,phCount)); % [phCount,Q]

%% Split Data into Train-Validate-Test sets
[trainInd,valInd,testInd] = dividerand(Q, 0.75, 0.25, 0);
fprintf('partioned Train-Valid-Test phones tally:\n')
[phoneIdsUnique' histc(phoneIds(trainInd), phoneIdsUnique)' histc(phoneIds(valInd), phoneIdsUnique)' histc(phoneIds(testInd), phoneIdsUnique)']

%% Find one-layer patternnet configuration
Layer1SizeMax = 100;
errTrainHist = zeros(1, Layer1SizeMax)-1;
errValHist = zeros(1, Layer1SizeMax)-1;
bestLayer1Size = [];
bestRecogRate = [];
for i=1:1000
    Layer1Size = randi([1 Layer1SizeMax], 1, 1, 'double');
    
    % This layer config has been already processed?
    if errTrainHist(Layer1Size) ~= -1
        continue;
    end

    % Train
    net = patternnet([Layer1Size]);
    net.divideParam.trainRatio=1;
    net.divideParam.valRatio=0;
    net.divideParam.testRatio=0;
    net.trainParam.epochs=1000;
    net.trainParam.min_grad=1e-6;
    [net,tr] = train(net, feats(:,trainInd), phonePatTarget(:,trainInd));

    % train error
    phoneIdProbs = net(feats(:,trainInd)); % [phCount,trainCount]
    recogRateTrain = accuracyTopN(phoneIdProbs,phoneIds(:,trainInd),1);
    errTrainHist(Layer1Size) = 1-recogRateTrain;

    % Validate
    phoneIdProbs = net(feats(:,valInd)); % [phCount,valCount]
    recogRateVal=accuracyTopN(phoneIdProbs,phoneIds(:,valInd),1);
    errValHist(Layer1Size) = 1-recogRateVal;
    
    fprintf('recogRateTrain=%f recogRateVal=%f net=%s t=%f\n', recogRateTrain, recogRateVal, mat2str(Layer1Size), max(tr.time));
    
    if isempty(bestRecogRate) || recogRateVal > bestRecogRate
        bestRecogRate = recogRateVal;
        bestLayer1Size = Layer1Size;
        display({bestRecogRate, Layer1Size});
    end
    
    figure(1)
    take = Layer1SizeMax;
    hasDataMask = errValHist ~= -1;
    hasDataMask = hasDataMask(1:take);
    x = 1:Layer1SizeMax;
    x = x(hasDataMask);
    plot(x, errTrainHist(hasDataMask), 'g', x, errValHist(hasDataMask), 'b')
end
%% Find two-layers patternnet configuration
Layer1Size = 40; % =const
Layer2SizeMax = 50;
errTrainHist = zeros(1, Layer2SizeMax)-1;
errValHist = zeros(1, Layer2SizeMax)-1;
bestLayer2Size = [];
bestRecogRate = [];
for i=1:1000
    Layer2Size = randi([1 Layer2SizeMax], 1, 1, 'double');
    
    % This layer config has been already processed?
    if errTrainHist(Layer2Size) ~= -1
        continue;
    end

    % Train
    net = patternnet([Layer1Size Layer2Size]);
    net.divideParam.trainRatio=1;
    net.divideParam.valRatio=0;
    net.divideParam.testRatio=0;
    net.trainParam.epochs=1000;
    net.trainParam.min_grad=1e-6;
    [net,tr] = train(net, feats(:,trainInd), phonePatTarget(:,trainInd));

    % train error
    phoneIdProbs = net(feats(:,trainInd)); % [phCount,trainCount]
    recogRateTrain = accuracyTopN(phoneIdProbs,phoneIds(:,trainInd),1);
    errTrainHist(Layer2Size) = 1-recogRateTrain;

    % Validate
    phoneIdProbs = net(feats(:,valInd)); % [phCount,valCount]
    recogRateVal=accuracyTopN(phoneIdProbs,phoneIds(:,valInd),1);
    errValHist(Layer2Size) = 1-recogRateVal;
    
    fprintf('recogRateTrain=%f recogRateVal=%f net=%s t=%f\n', recogRateTrain, recogRateVal, mat2str(Layer1Size), max(tr.time));
    
    if isempty(bestRecogRate) || recogRateVal > bestRecogRate
        bestRecogRate = recogRateVal;
        bestLayer2Size = Layer2Size;
        display({bestRecogRate, Layer2Size});
    end
    
    figure(1)
    hasDataMask = errValHist ~= -1;
    x = 1:Layer2SizeMax;
    x = x(hasDataMask);
    plot(x, errTrainHist(hasDataMask), 'g', x, errValHist(hasDataMask), 'b')
end
%% Test
%phoneIdProbs = net(feats(:,testInd)); % [phCount,testCount]
%recogRateVal=accuracyTopN(phoneIdProbs,phoneIds(:,testInd),1)
