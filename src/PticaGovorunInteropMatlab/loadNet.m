function net = loadNet(netPath)
    netStruct = load(netPath,'net');
    class(netStruct)
    net = netStruct.net;
    class(net)
end