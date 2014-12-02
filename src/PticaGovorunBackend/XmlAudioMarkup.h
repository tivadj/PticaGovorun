#pragma once
#include <QObject>
#include <tuple>
#include <string>
#include <vector>
#include "stdafx.h"
#include "SpeechProcessing.h" // TimePointMarker

namespace PticaGovorun {

class XmlAudioMarkup
{
public:
	XmlAudioMarkup();
	~XmlAudioMarkup();
};


PG_EXPORTS std::tuple<bool,const char*> loadAudioMarkupFromXml(const std::wstring& audioFilePathAbs, std::vector<TimePointMarker>& syncPoints);

}