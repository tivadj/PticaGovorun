#include "stdafx.h"
#include <vector>
#include <array>
#include <regex>
#include <cstring> // strtok
#include <string.h> // strtok
#include <cstdlib>
#include <hash_set>
#include <QFile>
#include <QString>
#include <QDomDocument>
#include <QStringList>
#include <QTextStream>
#include "XmlAudioMarkup.h"

namespace PticaGovorun {

namespace {
	static const char* XmlDocName = "audioAnnotation";
	static const char* MarkerName = "syncPoint";
	static const char* MarkerIdName = "id";
	static const char* MarkerSampleIndName = "sampleInd";
	static const char* MarkerTranscripTextName = "transcripText";
	static const char* MarkerLevelOfDetailName = "levelOfDetail";
	static const char* MarkerLevelOfDetailWordName = "word";
	static const char* MarkerLevelOfDetailPhoneName = "phone";
}

std::tuple<bool, const char*> loadAudioMarkupFromXml(const std::wstring& audioMarkupFilePathAbs, std::vector<TimePointMarker>& syncPoints)
{
	QFile file(QString::fromStdWString(audioMarkupFilePathAbs));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return std::make_tuple(false, "Can't open file");

	QDomDocument xml;
	if (!xml.setContent(&file))
		return std::make_tuple(false, "Can't open QDomDocument from file");;

	file.close();

	QDomElement docElem = xml.documentElement();
	//qDebug() << docElem.tagName();

	//
	std::hash_set<int> usedIds;
	QDomNodeList nodeList = docElem.childNodes();
	for (int i = 0; i<nodeList.count(); ++i)
	{
		QDomNode n = nodeList.at(i);
		QDomElement e = n.toElement();
		if (e.isNull())
			continue;
		if (e.tagName() == MarkerName)
		{
			bool parseIntOp;
			
			int markerId = e.attribute(MarkerIdName, "").toInt(&parseIntOp);
			if (!parseIntOp)
			{
				// generate random id
				while (true)
				{
					markerId = 1 + rand() % 400; // +1 for id>0
					if (usedIds.find(markerId) == std::end(usedIds))
						break;
				}
				usedIds.insert(markerId);
				
				//return std::make_tuple(false, "Xml audio markup error: expected syncPoint.id of integer type");
			}

			int sampleInd = e.attribute(MarkerSampleIndName, "").toInt(&parseIntOp);
			if (!parseIntOp)
				return std::make_tuple(false, "Xml audio markup error: expected syncPoint.sampleInd of integer type");
			
			QString transcripText = e.attribute(MarkerTranscripTextName, "");

			QString levelOfDetailStr = e.attribute(MarkerLevelOfDetailName, MarkerLevelOfDetailWordName);

			MarkerLevelOfDetail levelOfDetail = MarkerLevelOfDetail::Word;
			if (levelOfDetailStr == MarkerLevelOfDetailWordName)
				levelOfDetail = MarkerLevelOfDetail::Word;
			else if (levelOfDetailStr == MarkerLevelOfDetailPhoneName)
				levelOfDetail = MarkerLevelOfDetail::Phone;

			//
			PG_Assert(markerId != -1);

			TimePointMarker syncPoint;
			syncPoint.Id = markerId;
			syncPoint.SampleInd = sampleInd;
			syncPoint.TranscripText = transcripText;
			syncPoint.LevelOfDetail = levelOfDetail;

			// UI specific
			syncPoint.IsManual = true;
			syncPoint.StopsPlayback = getDefaultMarkerStopsPlayback(levelOfDetail);

			syncPoints.push_back(syncPoint);
		}
	}

	return std::make_tuple(true, nullptr);
}

PG_EXPORTS std::tuple<bool, const char*> saveAudioMarkupToXml(const std::vector<TimePointMarker>& syncPoints, const std::wstring& audioMarkupFilePathAbs)
{
	QFile file(QString::fromStdWString(audioMarkupFilePathAbs));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return std::make_tuple(false, "Can't open file for writing");

	QDomDocument xml;
	QDomNode node(xml.createProcessingInstruction("xml", R"pre(version="1.0" encoding="utf-8")pre"));
	xml.insertBefore(node, xml.firstChild());

	QDomElement docElem = xml.createElement(XmlDocName);
	xml.appendChild(docElem);
	
	for (const TimePointMarker& marker : syncPoints)
	{
		QDomElement syncPointNode = xml.createElement(MarkerName);
		syncPointNode.setAttribute(MarkerIdName, marker.Id);
		syncPointNode.setAttribute(MarkerSampleIndName, marker.SampleInd);

		if (!marker.TranscripText.isEmpty())
			syncPointNode.setAttribute(MarkerTranscripTextName, marker.TranscripText);
		
		QString levelOfDetailStr = "error";
		if (marker.LevelOfDetail == MarkerLevelOfDetail::Word)
			levelOfDetailStr = MarkerLevelOfDetailWordName;
		else if (marker.LevelOfDetail == MarkerLevelOfDetail::Phone)
			levelOfDetailStr = MarkerLevelOfDetailPhoneName;
		syncPointNode.setAttribute(MarkerLevelOfDetailName, levelOfDetailStr);

		docElem.appendChild(syncPointNode);
	}

	QTextStream textStream(&file);
	int indent = 3;
	xml.save(textStream, indent);

	return std::make_tuple(true, nullptr);
}

std::tuple<bool, const char*> loadPronunciationVocabulary(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec)
	{
		// file contains text in Windows-1251 encoding
		QFile file(QString::fromStdWString(vocabFilePathAbs));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file");
		
		// 
		std::array<char, 1024> lineBuff;

		// each line has a such format:
		// sure\tsh u e\n
		while (true) {
			auto readBytes = file.readLine(lineBuff.data(), lineBuff.size());
			if (readBytes == -1) // EOF
			{
				break;
			}

			// read the first word

			char* pMutStr = lineBuff.data(); // note, strtok modifies the buffer
			char* pMutStrNext = nullptr;

			const char* DictDelim = " \t\n";
			
			// strtok seems to be quicker than std::regex or QString::split approaches
			pMutStr = strtok_s(pMutStr, DictDelim, &pMutStrNext);
			if (pMutStr == nullptr)
			{
				// the line contains only the whitespace
				continue;
			}

			QString word = textCodec.toUnicode(pMutStr);

			// read the tail of phones
			std::vector<std::string> phones;
			while (true)
			{
				pMutStr = strtok_s(nullptr, DictDelim, &pMutStrNext);
				if (pMutStr == nullptr)
					break;

				auto len = strlen(pMutStr);
				std::string phoneStr(pMutStr, len);
				phones.push_back(std::move(phoneStr));
			}

			wordToPhoneList.insert(std::make_pair<std::wstring, std::vector<std::string>>(word.toStdWString(), std::move(phones)));
		}

		return std::make_tuple(true, nullptr);
	}
}