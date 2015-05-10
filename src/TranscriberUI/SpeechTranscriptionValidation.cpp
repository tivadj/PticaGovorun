#include "stdafx.h"
#include "SpeechAnnotation.h"
#include <vector>
#include "CoreUtils.h"
#include "SpeechTranscriptionValidation.h"

namespace PticaGovorun
{
	void validateSpeechAnnotation(const SpeechAnnotation& annot, PhoneticDictionaryViewModel& phoneticDictModel, QStringList& checkMsgs)
	{
		// validate markers structure

		annot.validateMarkers(checkMsgs);

		// validate that all pronId of entire transcript text are in the phonetic dictionary

		phoneticDictModel.validateSpeechAnnotationsHavePhoneticTranscription(annot, checkMsgs);
	}

	void validateAllOnDiskSpeechAnnotations(boost::wstring_ref speechDataDir, PhoneticDictionaryViewModel& phoneticDictModel, QStringList& checkMsgs)
	{
		QString curDirQ = toQString(speechDataDir);

		AnnotSpeechDirNode annotStructure;
		populateAnnotationFileStructure(curDirQ, annotStructure);

		std::vector<AnnotSpeechFileNode> annotInfos;
		flat(annotStructure, annotInfos);

		// do not consider badly marked markup
		// TODO: fix finance.ua-pynzenykvm markup
		std::remove_if(annotInfos.begin(), annotInfos.end(), [](AnnotSpeechFileNode& a)
		{
			return a.SpeechAnnotationXmlFilePath.contains("finance.ua-pynzenykvm");
		});

		for (const AnnotSpeechFileNode& fileItem : annotInfos)
		{
			SpeechAnnotation annot;
			bool loadOp;
			const char* errMsg;
			std::tie(loadOp, errMsg) = loadAudioMarkupFromXml(fileItem.SpeechAnnotationXmlFilePath.toStdWString(), annot);
			if (!loadOp)
			{
				checkMsgs.push_back(QString::fromLatin1(errMsg));
				continue;
			}

			QStringList fileItemMsgs;
			validateSpeechAnnotation(annot, phoneticDictModel, fileItemMsgs);

			if (!fileItemMsgs.empty())
			{
				checkMsgs << QString::fromStdWString(L"File=") << fileItem.SpeechAnnotationXmlFilePath << "\n";
				checkMsgs << fileItemMsgs << "\n";
			}
		}
	}

}