#include <QDir>
#include <QDirIterator>
#include <QDebug>

#include "XmlAudioMarkup.h"

namespace MigrateXmlSpeechAnnotRunnerNS
{
	using namespace PticaGovorun;
	void transformXmlAnnot()
	{
		SpeechAnnotation speechAnnot;

		const char* dirPath = R"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAnnot\)path";
		QDirIterator it(dirPath, QStringList() << "*.xml", QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			QString xmlPath = it.next();
			qDebug() << xmlPath;

			speechAnnot.clear();
			bool op;
			const char* errMsg;
			std::tie(op, errMsg) = PticaGovorun::loadAudioMarkupFromXml(xmlPath.toStdWString(), speechAnnot);
			if (!op)
			{
				qDebug() <<errMsg;
				return;
			}

			// add lang attr
			for (int markerInd = 0; markerInd < speechAnnot.markersSize(); ++markerInd)
			{
				PticaGovorun::TimePointMarker& m = speechAnnot.marker(markerInd);

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

			std::tie(op, errMsg) = PticaGovorun::saveAudioMarkupToXml(speechAnnot, xmlPath.toStdWString());
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