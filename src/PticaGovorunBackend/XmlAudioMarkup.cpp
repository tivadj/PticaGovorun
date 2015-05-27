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

#include "PticaGovorunCore.h"
#include "XmlAudioMarkup.h"

namespace PticaGovorun {

namespace {
	static const char* XmlDocName = "audioAnnotation";
	static const char* AnnotHeaderName = "header";
	static const char* HeaderSpeakerNodeName = "speaker";
	static const char* HeaderSpeakerBriefIdName = "briefId";
	static const char* HeaderSpeakerNameAttrName = "name";
	static const char* MarkerName = "syncPoint";
	static const char* MarkerIdName = "id";
	static const char* MarkerSampleIndName = "sampleInd";
	static const char* MarkerTranscripTextName = "transcripText";
	static const char* MarkerLanguageName = "lang";
	static const char* MarkerExcludePhaseName = "exclude";
	static const char* MarkerLangUkrainian = "uk";
	static const char* MarkerLangRussian = "ru";
	static const char* MarkerLevelOfDetailName = "levelOfDetail";
	static const char* MarkerLevelOfDetailWordName = "word";
	static const char* MarkerLevelOfDetailPhoneName = "phone";
	static const char* MarkerSpeakerBriefIdName = "speakerBriefId";
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
		QString tagName = e.tagName();
		if (tagName == AnnotHeaderName)
		{
			QDomNodeList headerChildList = n.childNodes();
			for (int headInd = 0; headInd<headerChildList.count(); ++headInd)
			{
				QDomElement headChild = headerChildList.at(headInd).toElement();
				QString headChildName = headChild.tagName();
				if (headChildName == HeaderSpeakerNodeName)
				{
					QString briefId = headChild.attribute(HeaderSpeakerBriefIdName, "");
					if (briefId.isEmpty())
						return std::make_tuple(false, "Xml audio markup error: expected non empty Speaker.Id");
					
					QString speakerName = headChild.attribute(HeaderSpeakerNameAttrName, "");
					annot.addSpeaker(briefId.toStdWString(), speakerName.toStdWString());
				}
			}
		}
		else if (tagName == MarkerName)
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

			QString speakerBriefIdStr = e.attribute(MarkerSpeakerBriefIdName, "");

			QString excludeStr = e.attribute(MarkerExcludePhaseName, "");
			boost::optional<ResourceUsagePhase> excludePhase = resourceUsagePhaseFromString(excludeStr.toStdString());

			//
			PG_Assert(markerId != -1);

			TimePointMarker syncPoint;
			syncPoint.Id = markerId;
			syncPoint.SampleInd = sampleInd;
			syncPoint.TranscripText = transcripText;
			syncPoint.LevelOfDetail = levelOfDetail;
			syncPoint.Language = lang;
			syncPoint.SpeakerBriefId = speakerBriefIdStr.toStdWString();
			syncPoint.ExcludePhase = excludePhase;

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

	if (!annot.speakers().empty())
	{
		xmlWriter.writeStartElement(AnnotHeaderName);
		for (const SpeakerFantom& speaker : annot.speakers())
		{
			xmlWriter.writeStartElement(HeaderSpeakerNodeName);
			xmlWriter.writeAttribute(HeaderSpeakerBriefIdName, QString::fromStdWString(speaker.BriefId));
			xmlWriter.writeAttribute(HeaderSpeakerNameAttrName, QString::fromStdWString(speaker.Name));
			xmlWriter.writeEndElement(); // HeaderSpeakerNodeName
		}
		xmlWriter.writeEndElement(); // AnnotHeaderName
	}
	
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

		if (!marker.SpeakerBriefId.empty())
		{
			QString spkStr = QString::fromStdWString(marker.SpeakerBriefId);
			xmlWriter.writeAttribute(MarkerSpeakerBriefIdName, spkStr);
		}

		if (marker.ExcludePhase != nullptr)
		{
			boost::string_ref excludeRef = toString(marker.ExcludePhase.get());
			QString excludeStrQ = QString::fromLatin1(excludeRef.data(), excludeRef.size());
			xmlWriter.writeAttribute(MarkerExcludePhaseName, excludeStrQ);
		}

		xmlWriter.writeEndElement(); // MarkerName
	}

	xmlWriter.writeEndElement(); // XmlDocName
	xmlWriter.writeEndDocument();

	return std::make_tuple(true, nullptr);
}

}