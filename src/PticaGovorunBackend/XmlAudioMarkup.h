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

PG_EXPORTS std::tuple<bool, const char*> loadWordToPhoneListVocabulary(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec);
PG_EXPORTS std::tuple<bool, const char*> loadWordToPhoneListVocabularyHashMap(const std::wstring& vocabFilePathAbs, std::hash_map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec);
PG_EXPORTS std::tuple<bool, const char*> loadWordToPhoneListVocabularyRegexHashMap(const std::wstring& vocabFilePathAbs, std::hash_map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec);
PG_EXPORTS std::tuple<bool, const char*> loadWordToPhoneListVocabularyRegexMap(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec);
PG_EXPORTS std::tuple<bool, const char*> loadWordToPhoneListVocabularyStrtok(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec);
}