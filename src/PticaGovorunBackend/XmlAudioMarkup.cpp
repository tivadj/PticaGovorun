#include "stdafx.h"
#include <vector>
#include <QFile>
#include <QDomDocument>
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

}