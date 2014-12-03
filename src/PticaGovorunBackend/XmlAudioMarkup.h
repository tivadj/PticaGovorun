#pragma once
#include <tuple>
#include <string>
#include <vector>
#include "stdafx.h"
#include <QObject>
#include <QTextCodec>
#include "SpeechProcessing.h" // TimePointMarker

namespace PticaGovorun {

class XmlAudioMarkup
{
public:
	XmlAudioMarkup();
	~XmlAudioMarkup();
};


PG_EXPORTS std::tuple<bool,const char*> loadAudioMarkupFromXml(const std::wstring& audioFilePathAbs, std::vector<TimePointMarker>& syncPoints);

PG_EXPORTS std::tuple<bool, const char*> loadWordToPhoneListVocabulary(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec);

}