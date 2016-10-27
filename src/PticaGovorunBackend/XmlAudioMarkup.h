#pragma once
#include <tuple>
#include <boost/filesystem.hpp>
#include "PticaGovorunCore.h" // PG_EXPORTS
#include "SpeechProcessing.h" // TimePointMarker
#include "SpeechAnnotation.h"
#include "ComponentsInfrastructure.h"

namespace PticaGovorun {

PG_EXPORTS std::tuple<bool, const char*> loadAudioMarkupFromXml(const std::wstring& audioFilePathAbs, SpeechAnnotation& speechAnnot);
PG_EXPORTS bool loadAudioMarkupXml(const boost::filesystem::path& audioFilePathAbs, SpeechAnnotation& speechAnnot, ErrMsgList* errMsg);
PG_EXPORTS std::tuple<bool, const char*> saveAudioMarkupToXml(const SpeechAnnotation& annot, const std::wstring& audioFilePathAbs);
}
