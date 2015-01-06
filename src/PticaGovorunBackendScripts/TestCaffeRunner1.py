import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import os
import sys
import caffe

def testMnist():
    modelPath = os.path.normpath(os.path.join(caffe_root,"examples/mnist/lenet.prototxt"))
    #modelPath = os.path.normpath(os.path.join(caffe_root,"examples/mnist/lenet_train_test_my.prototxt"))
    weightsPath = os.path.normpath(os.path.join(caffe_root,"examples/mnist/lenet_iter_10000.caffemodel"))

    # modelPath = os.path.normpath(os.path.join(caffe_root,  "examples/siamese/mnist_siamese_train_test.prototxt"))
    # weightsPath = os.path.normpath(os.path.join(caffe_root,"examples/siamese/mnist_siamese_iter_50000.caffemodel"))

    print(modelPath, weightsPath)

    net = caffe.Classifier(modelPath, weightsPath, image_dims=(28,28), mean=None, raw_scale=255, channel_swap=None) # mnist
    net.set_phase_test()
    net.set_mode_cpu()

    print(net.image_dims)
    print(net.blobs)
    print('a1',net.blobs['conv1'].data.shape)
    print('a1',net.blobs['conv1'].data[4,0:1])


    #
    testDataPath = os.path.normpath(os.path.join(caffe_root,'data/mnist/t10k-images-idx3-ubyte'))
    testLabelPath = os.path.normpath(os.path.join(caffe_root,'data/mnist/t10k-labels-idx1-ubyte'))
    n = 10000

    with open(testDataPath, 'rb') as f:
        f.read(16) # skip the header
        raw_data = np.fromstring(f.read(n * 28*28), dtype=np.uint8)

    imgListMat = raw_data.reshape(n, 28, 28)

    with open(testLabelPath, 'rb') as f:
        f.read(8) # skip the header
        labels = np.fromstring(f.read(n), dtype=np.uint8)

    #
    # take=10
    # for i in range(take):
    #     #print([i,labels[i],str(labels[i])])
    #     plt.subplot(1,take,i+1)
    #     plt.title(str(labels[i]))
    #     plt.imshow(imgList[i], cmap=cm.Greys_r)
    # plt.show()

    #first=imgList[0]
    #i0=first.reshape(28,28,1) # (H,W,K)
    #print(i0.shape)
    #print(i0[0].shape)
    #prediction = net.predict([i0, i0], oversample=False)

    imgListFixDim = imgListMat.reshape(n, 28, 28, 1)
    # cont=False
    # ind=0
    # while cont:
    #     plt.imshow(imgListMat[ind], cmap=cm.Greys_r)

    #prediction = net.predict(imgListFixDim, oversample=False)
    #i1 = np.array(imgListMat[ind]).reshape(28,28,1)
    take=1000
    imgList = [np.array(mat1).reshape(28,28,1) for mat1 in imgListMat[0:take]]
    prediction = net.predict(imgList, oversample=False)

    predictedLabels = prediction.argmax(1)
    correct=np.sum(predictedLabels==labels[0:take])
    accuracy=correct / float(len(labels))
    print("correct=%d accuracy=%f" %(correct,accuracy))
    intz=0

def testImnet():
    modelPath = os.path.normpath(os.path.join(caffe_root,  "models/bvlc_reference_caffenet/deploy.prototxt"))
    weightsPath = os.path.normpath(os.path.join(caffe_root,"models/bvlc_reference_caffenet/bvlc_reference_caffenet.caffemodel"))

    print(modelPath, weightsPath)

    meanMat = np.load('python/caffe/imagenet/ilsvrc_2012_mean.npy')
    net = caffe.Classifier(modelPath, weightsPath, image_dims=(256,256), mean=meanMat, raw_scale=255, channel_swap=(2,1,0)) # imagenet
    net.set_phase_test()
    net.set_mode_cpu()

    print(net.image_dims)
    print(net.blobs)
    print('a1',net.blobs['conv1'].data.shape)
    print('a1',net.blobs['conv1'].data[4,0:1])

    #
    grayToColor = True
    img = caffe.io.load_image('examples/images/cat.jpg', grayToColor)
    plt.imshow(img)
    plt.show()
    print(img.shape)

    prediction = net.predict([img], oversample=False)
    predictedLabels = prediction.argmax(1)
    print(predictedLabels)

    prediction = net.predict([img], oversample=True)
    predictedLabels = prediction.argmax(1)
    print(predictedLabels)

    intz=0


os.chdir('C:\\devb\\cplex\\caffe\\') # path with exe/dlls
caffe_root='C:\\devb\\cplex\\caffe\\'
sys.path.insert(0, caffe_root + 'python')

#testMnist()
testImnet()