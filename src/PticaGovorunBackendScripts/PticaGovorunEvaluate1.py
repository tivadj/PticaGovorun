import ctypes as C
import numpy as np

print("test")

#pticaGovorun = CDLL("../../build/x64/Debug/PticaGovorundBackendD.dll") # ERROR
pg = C.CDLL("C:\\devb\\PticaGovorunProj\\srcrep\\build\\x64\\Debug\\PticaGovorunBackendd")
#pticaGovorun = CDLL("PticaGovorunBackendd")
#pticaGovorun = CDLL("PticaGovorunBackendd.dll")
print(pg)

pg.loadPhoneToMfccFeaturesMap.argtypes = [C.c_int, C.c_wchar_p, C.c_int, C.c_int, C.c_int]
pg.loadPhoneToMfccFeaturesMap.restype = C.c_bool

def evaluateMonophoneClassifier(classifierId, feats, mfccVecLen, framesCount, phoneIds, logProbs):
    return pg.evaluateMonophoneClassifier(classifierId, feats.ctypes.data_as(C.POINTER(C.c_float)), mfccVecLen, framesCount, phoneIds.ctypes.data_as(C.POINTER(C.c_int)), logProbs.ctypes.data_as(C.POINTER(C.c_double)))

def reshapeAsTable(phoneToFeatsMap, mfccVecLen):
    rowsCount = pg.reshapeAsTable_GetRowsCount(phoneToFeatsMap, mfccVecLen)
    if rowsCount == -1:
        return False

    feats = np.zeros((rowsCount,mfccVecLen), np.float32)
    truePhoneIds = np.zeros((rowsCount,1), np.int32)
    reqDataOp = pg.reshapeAsTable(phoneToFeatsMap, mfccVecLen, rowsCount, feats.ctypes.data_as(C.POINTER(C.c_float)), truePhoneIds.ctypes.data_as(C.POINTER(C.c_int)))
    if not reqDataOp:
        return False
    return True, feats, truePhoneIds


def evaluate():
    phoneToFeatsMap = pg.createPhoneToMfccFeaturesMap()

    print("Loading features")
    wavRoot = R"E:/devb/workshop/PticaGovorunProj/data/prokopeo_specific/"
    frameSize = 400
    frameShift = 160
    mfccVecLen = 39
    loadOp = pg.loadPhoneToMfccFeaturesMap(phoneToFeatsMap, wavRoot, frameSize, frameShift, mfccVecLen)
    if not loadOp:
        return

    reqDataOp,feats,truePhoneIds = reshapeAsTable(phoneToFeatsMap, mfccVecLen)
    if not reqDataOp:
        return

    featsTable = np.hstack((feats, truePhoneIds))
    np.savetxt("speech_MFCC1.txt", featsTable, delimiter=",")


    for numClusters in range(15,40):

        #print("Training classifiers numClusters=%d" % numClusters)
        #numClusters = 1
        classifierId = C.c_int(-1)
        trainOp = pg.trainMonophoneClassifier(phoneToFeatsMap, mfccVecLen, numClusters, C.byref(classifierId))
        if not trainOp:
            return

        #print("classifierId=%d\n" % classifierId.value)

        framesCount = feats.shape[0]
        phoneIds = np.zeros((1,framesCount), np.int32)
        logProbs = np.zeros((1,framesCount), np.float32)
        evalOp = evaluateMonophoneClassifier(classifierId, feats, mfccVecLen, framesCount, phoneIds, logProbs)
        if not evalOp:
            return

        matchMask = phoneIds == truePhoneIds
        correct = np.sum(matchMask)
        accur = correct / framesCount
        print("numClusters=%d Total=%d Correct=%d Accur=%f" % (numClusters, framesCount, correct,accur))

    print("evaluate completed")

#pg.test2.argtypes = [c_int, POINTER(c_float)]
#pg.test2.restype = c_int
#feats = np.random.randn(1,2).astype(dtype=np.float32)
#phoneIds = np.zeros((1,2), np.int32)
#pg.test2(2, feats.ctypes.data_as(POINTER(c_float)))

evaluate()