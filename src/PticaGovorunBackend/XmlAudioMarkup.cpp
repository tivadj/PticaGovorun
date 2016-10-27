#include <vector>
#include <regex>
#include <cstdlib>
#include <QString>
#include <QDomDocument>
#include <QTextStream>
#include <QXmlStreamWriter>

#include "assertImpl.h"
#include "XmlAudioMarkup.h"

namespace PticaGovorun {

namespace {
	static const char* XmlDocName = "audioAnnotation";
	static const char* AnnotAudioFile = "audioFile";
	static const char* AnnotAudioSampleRate = "audioSampleRate";
	static const char* AnnotHeaderName = "header";
	static const char* HeaderSpeakerNodeName = "speaker";
	static const char* HeaderParameterNodeName = "param";
	static const char* HeaderParameterNameAttrName = "name";
	static const char* HeaderParameterValueAttrName = "value";
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
	QString audioFileQ = docElem.attribute(AnnotAudioFile, "");
	annot.setAudioFileRelPath(audioFileQ.toStdWString());
	
	//
	QString sampleRateQ = docElem.attribute(AnnotAudioSampleRate, "");
	bool opConv = false;
	float audioSampleRate = sampleRateQ.toFloat(&opConv);
	if (!opConv)
		return std::make_tuple(false, "Can't parse audioSampleRate parameter");;
	annot.setAudioSampleRate(audioSampleRate);

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
				else if (headChildName == HeaderParameterNodeName)
				{
					QString paramName = headChild.attribute(HeaderParameterNameAttrName, "");
					if (paramName.isEmpty())
						return std::make_tuple(false, "Xml audio markup error: parameter name is empty");
					
					QString paramValue = headChild.attribute(HeaderParameterValueAttrName, "");
					annot.addParameter(toUtf8StdString(paramName), toUtf8StdString(paramValue));
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

bool loadAudioMarkupXml(const boost::filesystem::path& audioFilePathAbs, SpeechAnnotation& speechAnnot, ErrMsgList* errMsg)
{
	bool loadOp;
	const char* cerr;
	std::tie(loadOp, cerr) = loadAudioMarkupFromXml(audioFilePathAbs.wstring(), speechAnnot);
	if (!loadOp && errMsg != nullptr)
		errMsg->utf8Msg = std::string(cerr);
	return loadOp;
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
	xmlWriter.writeAttribute(AnnotAudioFile, QString::fromStdWString(annot.audioFilePathRel()));
	bool isInt = annot.audioSampleRate() == std::truncf(annot.audioSampleRate());
	int prec = isInt ? 0 : 4; // set 0 for integers to avoid padding zeros after comma
	xmlWriter.writeAttribute(AnnotAudioSampleRate, QString::number(annot.audioSampleRate(), 'f', prec));

	if (!annot.speakers().empty() || !annot.parameters().empty())
	{
		xmlWriter.writeStartElement(AnnotHeaderName);
		for (const SpeakerFantom& speaker : annot.speakers())
		{
			xmlWriter.writeStartElement(HeaderSpeakerNodeName);
			xmlWriter.writeAttribute(HeaderSpeakerBriefIdName, QString::fromStdWString(speaker.BriefId));
			xmlWriter.writeAttribute(HeaderSpeakerNameAttrName, QString::fromStdWString(speaker.Name));
			xmlWriter.writeEndElement(); // HeaderSpeakerNodeName
		}
		for (const auto& param : annot.parameters())
		{
			xmlWriter.writeStartElement(HeaderParameterNodeName);
			xmlWriter.writeAttribute(HeaderParameterNameAttrName, utf8ToQString(param.Name));
			xmlWriter.writeAttribute(HeaderParameterValueAttrName, utf8ToQString(param.Value));
			xmlWriter.writeEndElement(); // HeaderParameterNodeName
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

		if (marker.ExcludePhase != boost::none)
		{
			boost::string_view excludeRef = toString(marker.ExcludePhase.get());
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