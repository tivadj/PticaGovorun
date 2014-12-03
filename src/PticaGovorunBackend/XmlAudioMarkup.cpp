#include "stdafx.h"
#include <vector>
#include <array>
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
		QFile file(QString::fromStdWString(vocabFilePathAbs));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file");
		

		std::array<char, 1024> lineBuff;

		// each line has a such format:
		// sure\tsh u e\n
		while (true) {
			auto readBytes = file.readLine(lineBuff.data(), lineBuff.size());
			if (readBytes == -1) // EOF
			{
				break;
			}
			//if (readBytes < 3) // skip blank lines
			//	continue;
			// TODO: split characters
			QString line = textCodec.toUnicode(lineBuff.data(), readBytes - 1); // -1 to trim '\n' character
			QStringList lineParts = line.split('\t', QString::SkipEmptyParts);
			if (lineParts.empty()) // skip blank lines
				continue;
			
			QString& word = lineParts[0];
			
			QStringList phoneListParts = lineParts[1].split(' ', QString::SkipEmptyParts);
			
			std::vector<std::string> phones;
			phones.reserve(phoneListParts.size());
			std::transform(std::begin(phoneListParts), std::end(phoneListParts), std::back_inserter(phones),
				[](const QString& phone)-> std::string { return phone.toStdString(); });

			wordToPhoneList.insert(std::make_pair<std::wstring, std::vector<std::string>>(word.toStdWString(), std::move(phones)));
		}

		return std::make_tuple(true, nullptr);
	}
}