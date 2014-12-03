#pragma once
#include <tuple>
#include <string>
#include <vector>
#include <hash_map>
#include <QObject>
#include <QTextCodec>
#include "PticaGovorunCore.h" // PG_EXPORTS
#include "SpeechProcessing.h" // TimePointMarker

namespace PticaGovorun {

class XmlAudioMarkup
{
public:
	XmlAudioMarkup();
	~XmlAudioMarkup();
};


PG_EXPORTS std::tuple<bool,const char*> loadAudioMarkupFromXml(const std::wstring& audioFilePathAbs, std::vector<TimePointMarker>& syncPoints);

// Loads dictionary of word -> (phone list) from text file.
// File usually has 'voca' extension.
// File has Windows-1251 encodeding.
// Each word may have multiple pronunciations (1-* relation); for now we neglect it and store data into map (1-1 relation).
PG_EXPORTS std::tuple<bool, const char*> loadWordToPhoneListVocabulary(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec);
}