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

		std::map<boost::wstring_ref, int> pronIdToUsedCount;
		for (const AnnotSpeechFileNode& fileItem : annotInfos)
		{
			// do not consider badly marked markup
			// TODO: fix finance.ua-pynzenykvm markup
			bool isBadMarkup = fileItem.SpeechAnnotationPath.contains("finance.ua-pynzenykvm");

			SpeechAnnotation annot;
			bool loadOp;
			const char* errMsg;
			std::tie(loadOp, errMsg) = loadAudioMarkupFromXml(fileItem.SpeechAnnotationPath.toStdWString(), annot);
			if (!loadOp)
			{
				checkMsgs.push_back(QString::fromLatin1(errMsg));
				continue;
			}

			// count usage of pronIds in phonetic dictionary
			phoneticDictModel.countPronIdUsage(annot, pronIdToUsedCount);

			if (!isBadMarkup)
			{
				QStringList fileItemMsgs;
				validateSpeechAnnotation(annot, phoneticDictModel, fileItemMsgs);

				if (!fileItemMsgs.empty())
				{
					checkMsgs << QString::fromStdWString(L"File=") << fileItem.SpeechAnnotationPath << "\n";
					checkMsgs << fileItemMsgs << "\n";
				}
			}
		}

		//
		for (const auto& pair : pronIdToUsedCount)
		{
			if (pair.second == 0)
				checkMsgs << QString("PronId=%1 is not used in speech annotation").arg(toQString(pair.first)) << "\n";
		}
		phoneticDictModel.validateAllPronunciationsSpecifyStress(checkMsgs);
	}

}