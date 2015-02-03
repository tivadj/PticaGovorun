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
#include <QXmlStreamWriter>

#include "XmlAudioMarkup.h"

namespace PticaGovorun {

namespace {
	static const char* XmlDocName = "audioAnnotation";
	static const char* MarkerName = "syncPoint";
	static const char* MarkerIdName = "id";
	static const char* MarkerSampleIndName = "sampleInd";
	static const char* MarkerTranscripTextName = "transcripText";
	static const char* MarkerLanguageName = "lang";
	static const char* MarkerLangUkrainian = "uk";
	static const char* MarkerLangRussian = "ru";
	static const char* MarkerLevelOfDetailName = "levelOfDetail";
	static const char* MarkerLevelOfDetailWordName = "word";
	static const char* MarkerLevelOfDetailPhoneName = "phone";
}

std::tuple<bool, const char*> loadAudioMarkupFromXml(const std::wstring& audioMarkupFilePathAbs, SpeechAnnotation& annot)
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
				return std::make_tuple(false, "Xml audio markup error: expected syncPoint.id of integer type");

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

			QString langStr = e.attribute(MarkerLanguageName, "");
			SpeechLanguage lang = SpeechLanguage::NotSet;
			if (langStr == MarkerLangUkrainian)
				lang = SpeechLanguage::Ukrainian;
			else if (langStr == MarkerLangRussian)
				lang = SpeechLanguage::Russian;

			//
			PG_Assert(markerId != -1);

			TimePointMarker syncPoint;
			syncPoint.Id = markerId;
			syncPoint.SampleInd = sampleInd;
			syncPoint.TranscripText = transcripText;
			syncPoint.LevelOfDetail = levelOfDetail;
			syncPoint.Language = lang;

			// UI specific
			syncPoint.IsManual = true;
			syncPoint.StopsPlayback = getDefaultMarkerStopsPlayback(levelOfDetail);

			annot.attachMarker(syncPoint);
		}
	}

	return std::make_tuple(true, nullptr);
}

PG_EXPORTS std::tuple<bool, const char*> saveAudioMarkupToXml(const SpeechAnnotation& annot, const std::wstring& audioMarkupFilePathAbs)
{
	QFile file(QString::fromStdWString(audioMarkupFilePathAbs));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return std::make_tuple(false, "Can't open file for writing");

	QXmlStreamWriter xmlWriter(&file);
	xmlWriter.setCodec("utf-8");
	xmlWriter.setAutoFormatting(true);
	xmlWriter.setAutoFormattingIndent(3);

	xmlWriter.writeStartDocument("1.0");
	xmlWriter.writeStartElement(XmlDocName);
	
	for (const TimePointMarker& marker : annot.markers())
	{
		xmlWriter.writeStartElement(MarkerName);
		xmlWriter.writeAttribute(MarkerIdName, QString::number(marker.Id));
		xmlWriter.writeAttribute(MarkerSampleIndName, QString::number(marker.SampleInd));

		QString levelOfDetailStr = "error";
		if (marker.LevelOfDetail == MarkerLevelOfDetail::Word)
			levelOfDetailStr = MarkerLevelOfDetailWordName;
		else if (marker.LevelOfDetail == MarkerLevelOfDetail::Phone)
			levelOfDetailStr = MarkerLevelOfDetailPhoneName;
		xmlWriter.writeAttribute(MarkerLevelOfDetailName, levelOfDetailStr);

		if (!marker.TranscripText.isEmpty())
			xmlWriter.writeAttribute(MarkerTranscripTextName, marker.TranscripText);

		if (marker.Language != SpeechLanguage::NotSet)
		{
			std::string langStr = speechLanguageToStr(marker.Language);
			xmlWriter.writeAttribute(MarkerLanguageName, langStr.c_str());
		}

		xmlWriter.writeEndElement(); // MarkerName
	}

	xmlWriter.writeEndElement(); // XmlDocName
	xmlWriter.writeEndDocument();

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