#include "stdafx.h"
#include <vector>
#include <array>
#include <regex>
#include <cstring> // strtok
#include <string.h> // strtok
#include <QFile>
#include <QString>
#include <QDomDocument>
#include <QStringList>
#include "XmlAudioMarkup.h"

namespace PticaGovorun {

XmlAudioMarkup::XmlAudioMarkup()
{
}


XmlAudioMarkup::~XmlAudioMarkup()
{
}


std::tuple<bool, const char*> loadAudioMarkupFromXml(const std::wstring& audioFilePathAbs, std::vector<TimePointMarker>& syncPoints)
{
	QFile file(QString::fromStdWString(audioFilePathAbs));
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
		if (e.tagName() == "syncPoint")
		{
			bool parseIntOp;
			int sampleInd = e.attribute("sampleInd").toInt(&parseIntOp);
			if (!parseIntOp)
				return std::make_tuple(false, "Xml audio markup error: expected syncPoint.sampleInd of integer type");
			
			QString transcripText = e.attribute("transcripText", "");

			TimePointMarker syncPoint;
			syncPoint.SampleInd = sampleInd;
			syncPoint.TranscripText = transcripText;
			syncPoint.IsManual = true;
			syncPoints.push_back(syncPoint);
		}
	}

	return std::make_tuple(true, nullptr);
}

std::tuple<bool, const char*> loadWordToPhoneListVocabulary(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec)
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