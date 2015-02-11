#pragma once
#include <tuple>
#include <string>
#include <vector>
#include <hash_map>
#include <QObject>
#include <QTextCodec>
#include "PticaGovorunCore.h" // PG_EXPORTS
#include "SpeechProcessing.h" // TimePointMarker
#include "SpeechAnnotation.h"

namespace PticaGovorun {

PG_EXPORTS std::tuple<bool, const char*> loadAudioMarkupFromXml(const std::wstring& audioFilePathAbs, SpeechAnnotation& speechAnnot);
PG_EXPORTS std::tuple<bool, const char*> saveAudioMarkupToXml(const SpeechAnnotation& annot, const std::wstring& audioFilePathAbs);
}