%% load network
net4=loadNet('netPatternnet282.mat')

%%
table1=csvread('../../../data/speech_MFCC1.txt');
data1 = table1(1,1:end-1)
data1Float = single(data1);
simRes = simulateNet(net4, reshape(data1,[],1));
simResFloat = simulateNet(net4, reshape(data1Float,[],1));
fprintf(mat2str(simResFloat))

%%
simRes=loadNetAndSimulate('netPatternnet282.mat', data1');

%%
% store network
save('netPatternnet282.mat', 'net')

%%
% generate standalone M-function from the trained net
genFunction(net, 'simulateStandaloneNet.m', 'MatrixOnly','yes', 'ShowLinks','no')

%%
simulateStandaloneNet(data1')