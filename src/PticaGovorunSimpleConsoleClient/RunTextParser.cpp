#include "stdafx.h"
#include <iostream>
#include <string>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QTextStream>
#include <QXmlStreamReader>

#include "ClnUtils.h"

#include "TextProcessing.h"
#include "CoreUtils.h"

namespace RunTextParserNS
{
	using namespace PticaGovorun;

	void parseFile(const wchar_t* filePath)
	{
		QString filePathQ = QString::fromStdWString(filePath);
		QFile file(filePathQ);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			qDebug() << "Can't open file " << filePathQ;
			return;
		}

		QXmlStreamReader xml(&file);

		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);
		TextParser wordsReader;

		//

		std::stringstream dumpFileName;
		dumpFileName << "utterExtract.";
		appendTimeStampNow(dumpFileName);
		dumpFileName << ".txt";

		QFile dumpFile(dumpFileName.str().c_str());
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cerr << "Can't create ouput file '" << dumpFileName.str() << "'" << std::endl;
			return;
		}

		QTextStream dumpFileStream(&dumpFile);
		dumpFileStream.setCodec("UTF-8");


		while (!xml.atEnd())
		{
			xml.readNext();
			if (xml.isCharacters())
			{
				QStringRef elementText = xml.text();

				wv::slice<wchar_t> textToParse = wv::make_view((wchar_t*)elementText.data(), elementText.size());
				wordsReader.setInputText(textToParse);

				// extract all sentences from paragraph
				while (true)
				{
					words.clear();
					if (!wordsReader.parseSentence(words))
						break;

					if (words.empty())
						continue;

					dumpFileStream << "<s>";
					if (!words.empty())
					{
						std::wstring str = toString(words[0]);
						dumpFileStream << " " << QString::fromStdWString(str);
					}
					for (int i = 1; i < words.size(); ++i)
					{
						dumpFileStream << " ";
						std::wstring str = toString(words[i]);
						dumpFileStream << QString::fromStdWString(str);
					}
					dumpFileStream << " </s>" << endl;
				}
			}
		}

		if (xml.hasError()) {
			std::wcerr << L"Error in XML stucture: " << xml.errorString().toStdWString();
			return;
		}

		std::wcout << "File parsed successfully" << std::endl;
	}


	void runMain(int argc, wchar_t* argv[])
	{
		const wchar_t* filePath = LR"path(C:\devb\PticaGovorunProj\data\textWorld\fiction\„орний ¬орон. Ўкл€р ¬асиль.fb2)path";
		if (argc > 1)
			filePath = argv[1];

		parseFile(filePath);
	}
}
