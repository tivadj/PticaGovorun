#include "stdafx.h"
#include <QDir>
#include <QDirIterator>
#include <QDebug>

#include "XmlAudioMarkup.h"

namespace MigrateXmlSpeechAnnotRunnerNS
{
	void transformXmlAnnot()
	{
		std::vector<PticaGovorun::TimePointMarker> frameIndMarkers;

		const char* dirPath = R"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAnnot\)path";
		QDirIterator it(dirPath, QStringList() << "*.xml", QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			QString xmlPath = it.next();
			qDebug() << xmlPath;

			frameIndMarkers.clear();
			bool op;
			const char* errMsg;
			std::tie(op, errMsg) = PticaGovorun::loadAudioMarkupFromXml(xmlPath.toStdWString(), frameIndMarkers);
			if (!op)
			{
				qDebug() <<errMsg;
				return;
			}

			// add lang attr
			for (PticaGovorun::TimePointMarker& m : frameIndMarkers)
			{
				if (m.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Phone)
					m.Language = PticaGovorun::SpeechLanguage::NotSet;
				else
				{
					if (m.TranscripText.isEmpty())
						m.Language = PticaGovorun::SpeechLanguage::NotSet;
					else if (m.Language == PticaGovorun::SpeechLanguage::NotSet)
						m.Language = PticaGovorun::SpeechLanguage::Ukrainian;
				}
			}

			std::tie(op, errMsg) = PticaGovorun::saveAudioMarkupToXml(frameIndMarkers, xmlPath.toStdWString());
			if (!op)
			{
				qDebug() << errMsg;
				return;
			}
		}
		qDebug() << "Success";
	}
	void run()
	{
		transformXmlAnnot();
	}
}