#include <iostream>
#include <string>
#include <QDebug>
#include <QString>
#include <QXmlStreamReader>
#include <QApplication>
#include "PhoneticService.h"
#include "TextProcessing.h"
#include "AppHelpers.h"

namespace UkrainianPhoneticSplitterNS
{
	using namespace PticaGovorun;

	void run()
	{
		QString dirPath = QString::fromWCharArray(LR"path(C:\devb\PticaGovorunProj\TmpLang1Proj)path");
		QStringList args = QApplication::arguments();
		if (args.size() > 1)
			dirPath = args[1];

		//
		std::shared_ptr<SentenceParser> sentParser;
		std::shared_ptr<GrowOnlyPinArena<wchar_t>> arena;
		arena = std::make_unique<GrowOnlyPinArena<wchar_t>>(1024);
		sentParser = std::make_unique<SentenceParser>(1024);

		auto abbrExp = std::make_shared<AbbreviationExpanderUkr>();
		abbrExp->stringArena_ = arena;

		auto numDictPath = AppHelpers::mapPath("pgdata/LM_ua/numsCardOrd.xml").toStdWString();
		std::wstring errMsg;
		if (!abbrExp->load(numDictPath, &errMsg))
		{
			std::wcerr << L"Can't load numbers dictionary. " << errMsg;
			return;
		}
		sentParser->setAbbrevExpander(abbrExp);

		UkrainianPhoneticSplitter s;
		long totalPreSplitWords;
		s.setSentParser(sentParser);
		s.outputCorpus_ = true;
		s.corpusFilePath_ = L"TmpCorpus.txt";
		s.outputCorpusNormaliz_ = true;
		s.corpusNormalizFilePath_ = L"TmpCorpusNormaliz.txt";
		s.gatherWordPartsSequenceUsage(toBfs(dirPath), totalPreSplitWords, -1);
	}
}
