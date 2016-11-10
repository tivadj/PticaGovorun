import time
from datetime import datetime
import argparse
#import xml.etree.cElementTree as ET
import lxml.etree as ET

class UtterExpectActualResult:
    def __init__(self):
        self.relWavPath = []
        self.attributes = []
        self.subElements = []
    def propValue(self, propName):
        # try sub elements
        keyVal = next(filter(lambda t: t[0] == propName, self.subElements), None)
        if not keyVal is None:
            return keyVal[1]
        # try attributes
        keyVal = next(filter(lambda t: t[0] == propName, self.attributes), None)
        return keyVal[1]

# The set of segment's decoder results for the same segment but with different decoder versinos.
class UtterExpectActualResultGroup:
    def __init__(self):
        self.relWavPath = []
        self.instances = [] # list(SpeechSegmentExpectActualResult)

    def hasInstanceWithSameDecoderOutput(self, newItem):
        strNew = newItem.propValue("wordsActual")

        def matchExpectFun(u):
            strOld = u.propValue("wordsActual")
            return strOld == strNew

        items = filter(matchExpectFun, self.instances)
        return any(items)


def parseDecoderExpectActualResults(filePath):
    result = []
    with open(filePath, 'r', encoding="utf-8") as f:
        props = {}
        wavPath = []
        while True:
            line = f.readline()
            if not line: break

            if line == "\n":
                item = UtterExpectActualResult()
                item.relWavPath = wavPath
                item.subElements = props
                result.append(item)

                props = {}
                wavPath = []
                continue

            wavPathNameStr = "RelWavPath"
            wavPathInd =line.find(wavPathNameStr)
            if wavPathInd != -1:
                # +1 for equal sign
                # -1 to skip newline
                wavPath = line[wavPathInd+len(wavPathNameStr)+1:-1]
                continue

                # expectWords = []
                # actualWords = []
                #
                # while expectWords == [] or actualWords == []:
                #     line = f.readline()
                #
                #     expectWordsNameStr = "ExpectWords"
                #     actualWordsNameStr = "ActualWords"
                #
                #     if line.startswith(expectWordsNameStr):
                #         expectWords = line[len(expectWordsNameStr)+1:-1]
                #     elif line.startswith(actualWordsNameStr):
                #         actualWords = line[len(actualWordsNameStr)+1:-1]
            eqInd = line.find("=")
            if eqInd != -1:
                key = line[:eqInd]
                value = line[eqInd+1:-1]
                props[key] = value
                continue
    return result

def putToCache(decoderCacheDict, newItems):
    newCount = 0
    oldCount = 0
    for newItem in newItems:
        entry = decoderCacheDict.get(newItem.relWavPath, None)
        if not entry:
            newEntry = UtterExpectActualResultGroup()
            newEntry.relWavPath = newItem.relWavPath
            newEntry.instances.append(newItem)
            decoderCacheDict[newItem.relWavPath] = newEntry
            newCount += 1
            continue
        if entry.hasInstanceWithSameDecoderOutput(newItem):
            oldCount += 1
            continue

        entry.instances.append(newItem)
        newCount += 1
    return (newCount, oldCount)

def diff(baseCache, newItems):
    newDecUttersDict = {}
    newCount = 0
    oldCount = 0
    for newItem in newItems.values():
        # ignore correctly recognized utterances
        wordDist = int(newItem.propValue("wordDist"))
        if wordDist == 0:
            continue

        cacheUtterGroup = baseCache.get(newItem.relWavPath, None)

        # new segment?
        isNew =  cacheUtterGroup is None

        # new recognition result for this segment?
        if not isNew:
            isNew = not cacheUtterGroup.hasInstanceWithSameDecoderOutput(newItem)

        if isNew:
            newDecUttersDict[newItem.relWavPath] = newItem
            newCount += 1
        else:
            oldCount += 1
    return (newDecUttersDict, newCount, oldCount)

def readDecoderExpectActualResultsXml(filePath, utterDecInfo):

    with open(filePath,'r', encoding='utf-8') as f:
        tree = ET.parse(f)
        root = tree.getroot()
        for utterNode in root.iter("utter"):
            item = UtterExpectActualResult()
            item.relWavPath = utterNode.attrib["relWavPath"]

            # read attributes
            for keyVal in utterNode.attrib.items():
                item.attributes.append(keyVal)

            # read sub items
            for node in utterNode.getchildren():
                key = node.tag
                value = node.text
                item.subElements.append((key,value))
            utterDecInfo[item.relWavPath] = item
    return True

def writeDecoderExpectActualResultsXml(filePath, utterDecInfo):
    doc = ET.Element("decRecogResult")

    for utter in utterDecInfo.values():
        utterNode = ET.SubElement(doc, "utter",
                      relWavPath=utter.relWavPath)

        for keyVal in utter.attributes:
            utterNode.attrib[keyVal[0]] = keyVal[1]

        for keyVal in utter.subElements:
            utterChildNode = ET.SubElement(utterNode, keyVal[0])
            utterChildNode.text = keyVal[1]


    tree = ET.ElementTree(doc)
    #tree.write(filePath, encoding="utf-8")
    xmlStr = ET.tostring(tree, pretty_print=True, encoding='utf-8', xml_declaration=True)
    with open(filePath, "wb") as f:
        f.write(xmlStr)
    return True

# def readSpeechRecogMismatchCache(filePath, cache):
#
#     with open(filePath,'r') as f:
#         tree = ET.parse(f)
#         root = tree.getroot()
#         for utterNode in root.iter("utter"):
#             item = UtterExpectActualResult()
#             item.relWavPath = utterNode.attrib["relWavPath"]
#
#             for node in utterNode.getchildren():
#                 key = node.tag
#                 value = node.attrib["value"]
#                 item.subElements[key] = value
#             cache[item.relWavPath] = item
#     return True

def readDecoderResultsCacheXml(filePath, decCacheDict):

    with open(filePath,'r', encoding='utf-8') as f:
        tree = ET.parse(f)
        root = tree.getroot()
        for utterGroupNode in root.iter("utterGroup"):
            utterGroup = UtterExpectActualResultGroup()
            utterGroup.relWavPath = utterGroupNode.attrib["relWavPath"]

            for utterNode in utterGroupNode.iter("utter"):
                utter = UtterExpectActualResult()
                utter.relWavPath = utterNode.attrib["relWavPath"]

                # read attributes
                for keyVal in utterNode.attrib.items():
                    utter.attributes.append(keyVal)

                # read sub items
                for node in utterNode.getchildren():
                    key = node.tag
                    value = node.text
                    utter.subElements.append((key,value))
                utterGroup.instances.append(utter)

            decCacheDict[utterGroup.relWavPath] = utterGroup
    return True

def writeDecoderResultsCacheXml(cacheDict, filePath):
    doc = ET.Element("decRecogCache")

    for utterGroup in cacheDict.values():
        utterGroupNode = ET.SubElement(doc, "utterGroup",
                      relWavPath=utterGroup.relWavPath)

        for utter in utterGroup.instances:
            utterNode = ET.SubElement(utterGroupNode, "utter")

            for keyVal in utter.attributes:
                utterNode.attrib[keyVal[0]] = keyVal[1]

            for keyVal in utter.subElements:
                utterChildNode = ET.SubElement(utterNode, keyVal[0])
                utterChildNode.text = keyVal[1]


    tree = ET.ElementTree(doc)
    #tree.write(filePath, encoding="utf-8")
    xmlStr = ET.tostring(tree, pretty_print=True, encoding='utf-8', xml_declaration=True)
    with open(filePath, "wb") as f:
        f.write(xmlStr)

def main1():
    parser = argparse.ArgumentParser(description='Post process speech decoder mismatches.')
    parser.add_argument('op', type=str, default="diff", help='operation to perform') # put, diff
    parser.add_argument('file', help='input text file to process')
    args = parser.parse_args()

    decoderCacheDict = {}
    cachePath = r"C:\devb\PticaGovorunProj\srcrep\build\x64\Release\decRecogCache.xml"
    if not readDecoderResultsCacheXml(cachePath, decoderCacheDict):
        print("Can't read recognized speech cache")
        return

    decResultsPath = args.file
    decoderResultsDict = {}
    if not readDecoderExpectActualResultsXml(decResultsPath, decoderResultsDict):
        print("Can't read decoder output")
        return

    if args.op == "put":
        (newCount, oldCount) = putToCache(decoderCacheDict, decoderResultsDict.values())
        print("newUtters=%d new/all=%f" % (newCount, newCount/float(newCount+oldCount)))

        writeDecoderResultsCacheXml(decoderCacheDict, cachePath)
    elif args.op == "diff":
        (newDecUttersDict, newCount, oldCount) = diff(decoderCacheDict, decoderResultsDict)
        print("newUtters=%d new/all=%f" % (newCount, newCount/float(newCount+oldCount)))

        timeStamp = datetime.today().strftime("%f")
        writeDecoderExpectActualResultsXml(decResultsPath + "." + timeStamp, newDecUttersDict)
    print("")

# main diff errorDump.txt
# main put errorDump.txt
if __name__ == "__main__":
    main1()
    print("Main exited")