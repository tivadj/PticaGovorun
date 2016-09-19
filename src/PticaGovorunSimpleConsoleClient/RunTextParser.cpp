#include <iostream>
#include <string>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QApplication>
#include "TextProcessing.h"
#include "CoreUtils.h"
#include "PhoneticService.h"

namespace RunTextParserNS
{
	using namespace PticaGovorun;

	void parseFile(const QString& filePath)
	{
		QFile file(filePath);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			std::wcerr << "Can't open file " << filePath.toStdWString();
			return;
		}

		QXmlStreamReader xml(&file);

		std::vector<RawTextRun> words;
		words.reserve(64);
		std::vector<wchar_t> sentBuff;
		TextParser wordsReader;

		//

		std::stringstream dumpFileName;
		dumpFileName << "utterExtract.";
		appendTimeStampNow(dumpFileName);
		dumpFileName << ".txt";

		QFile dumpFile(dumpFileName.str().c_str());
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::wcerr << "Can't create ouput file '" << dumpFileName.str().c_str() << "'" << std::endl;
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

				sentBuff.clear();
				boost::wstring_view textToParse = toWStringRef(elementText, sentBuff);
				wordsReader.setInputText(textToParse);

				// extract all sentences from paragraph
				while (true)
				{
					words.clear();
					if (!wordsReader.parseTokensTillDot(words))
						break;

					if (words.empty())
						continue;

					dumpFileStream << "<s>";
					if (!words.empty())
					{
						std::wstring str = toString(words[0].Str);
						dumpFileStream << " " << QString::fromStdWString(str);
					}
					for (int i = 1; i < words.size(); ++i)
					{
						dumpFileStream << " ";
						std::wstring str = toString(words[i].Str);
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

	void run()
	{
		QString filePath = QString::fromWCharArray(LR"path(C:\devb\PticaGovorunProj\data\textWorld\fiction\„орний ¬орон. Ўкл€р ¬асиль.fb2)path");
		QStringList args = QApplication::arguments();
		if (args.size() > 1)
			filePath = args[1];

		parseFile(filePath);
	}
}
