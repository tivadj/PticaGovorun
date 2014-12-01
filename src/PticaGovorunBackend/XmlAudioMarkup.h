#pragma once
#include <tuple>
#include <QObject>
#include <string>
#include <vector>
#include "stdafx.h"

namespace PticaGovorun {

class XmlAudioMarkup
{
public:
	XmlAudioMarkup();
	~XmlAudioMarkup();
};

struct TimePointMarker
{
	long SampleInd;

	// Whether marker was added by user (manual=true). Automatic markers (manual=false) can be freely
	// removed from model without user's concern. For example, when audio is reanalyzed, old automatic markers
	// may be replaced with new ones.
	bool IsManual;

	QString TranscripText;

	// recognition results
	QString RecogSegmentText;
	QString RecogSegmentWords;
};

PG_EXPORTS std::tuple<bool,const char*> loadAudioMarkupFromXml(const std::wstring& audioFilePathAbs, std::vector<TimePointMarker>& syncPoints);

}