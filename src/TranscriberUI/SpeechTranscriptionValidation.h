#pragma once
#include <QStringList>
#include <boost/utility/string_ref.hpp>
#include "SpeechAnnotation.h"
#include "PhoneticDictionaryViewModel.h"

namespace PticaGovorun
{
	void validateAllOnDiskSpeechAnnotations(boost::wstring_ref speechDataDir, PhoneticDictionaryViewModel& phoneticDictModel, QStringList& checkMsgs);
	void validateSpeechAnnotation(const SpeechAnnotation& annot, PhoneticDictionaryViewModel& phoneticDictModel, QStringList& checkMsgs);
}