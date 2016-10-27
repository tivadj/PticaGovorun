#include <fstream>
#include <boost/format.hpp>
#include <QFile>
#include <QXmlStreamWriter>

#include "PhoneticService.h"
#include "assertImpl.h"

namespace
{
	const char* XmlDocTag = "phoneticDict";
	const char* WordTag = "word";
	const char* WordName = "name";
	const char* WordPhones = "phones";
	const char* PronunciationTag = "pron";
	const char* PronunciationCode = "code";
}

namespace PticaGovorun
{
	std::tuple<bool, const char*> savePhoneticDictionaryXml(const std::vector<PhoneticWord>& phoneticDict, boost::wstring_view filePath, const PhoneRegistry& phoneReg)
	{
		QFile file(toQString(filePath));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file for writing");

		QXmlStreamWriter xmlWriter(&file);
		xmlWriter.setCodec("utf-8");
		xmlWriter.setAutoFormatting(true);
		xmlWriter.setAutoFormattingIndent(3);

		xmlWriter.writeStartDocument("1.0");
		xmlWriter.writeStartElement(XmlDocTag);

		std::string phoneListStr;

		for (const PhoneticWord& wordPron : phoneticDict)
		{
			QString wordQ = toQString(wordPron.Word);

			xmlWriter.writeStartElement(WordTag);
			xmlWriter.writeAttribute(WordName, wordQ);

			// word and single pronunciation can be collapsed into single line
			// if pronAsWord is equal to word itself

			if (wordPron.Pronunciations.size() == 1 && wordPron.Word == wordPron.Pronunciations[0].PronCode)
			{
				phoneListStr.clear();
				phoneListToStr(phoneReg, wordPron.Pronunciations[0].Phones, phoneListStr);

				xmlWriter.writeAttribute(WordPhones, QString::fromStdString(phoneListStr));
			}
			else
			{
				// write all pronunciations
				for (const PronunciationFlavour& pron : wordPron.Pronunciations)
				{
					xmlWriter.writeStartElement(PronunciationTag);
						
					xmlWriter.writeAttribute(PronunciationCode, toQString(pron.PronCode));

					phoneListStr.clear();
					phoneListToStr(phoneReg, pron.Phones, phoneListStr);

					xmlWriter.writeAttribute(WordPhones, QString::fromStdString(phoneListStr));

					xmlWriter.writeEndElement(); // PronunciationTag
				}
			}
			xmlWriter.writeEndElement(); // WordTag
		}
		xmlWriter.writeEndElement(); // XmlDocTag

		return std::make_tuple(true, nullptr);
	}

	bool loadPhoneticDictionaryXml(const boost::filesystem::path& filePath, const PhoneRegistry& phoneReg, std::vector<PhoneticWord>& phoneticDict, GrowOnlyPinArena<wchar_t>& stringArena, ErrMsgList* errMsg)
	{
		QFile file(toQStringBfs(filePath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			if (errMsg != nullptr) errMsg->utf8Msg = str(boost::format("Can't open file for reading (%1%)") % filePath.string());
			return false;
		}

		QXmlStreamReader xml(&file);

		QTextStream dumpFileStream(&file);
		dumpFileStream.setCodec("UTF-8");

		std::vector<wchar_t> transientStringBuff;
		PhoneticWord wordPron;
		bool singlePronPerWord = false;

		auto maybeFinishPreviousWord = [&]() -> void
		{
			if (wordPron.Word.empty())
				return;

			phoneticDict.push_back(wordPron);

			wordPron.Word.clear();
			wordPron.Pronunciations.clear();
			singlePronPerWord = false;
		};

		while (!xml.atEnd())
		{
			xml.readNext();
			if (xml.isStartElement() && xml.name() == WordTag)
			{
				maybeFinishPreviousWord();

				boost::wstring_view arenaWordRef;

				const QXmlStreamAttributes& attrs = xml.attributes();
				if (attrs.hasAttribute(WordName))
				{
					QStringRef nameStr = attrs.value(WordName);
					boost::wstring_view name = toWStringRef(nameStr, transientStringBuff);
					
					arenaWordRef = registerWordThrow2(stringArena, name);

					wordPron.Word = arenaWordRef;
				}
				if (attrs.hasAttribute(WordPhones))
				{
					QStringRef phonesQRef = attrs.value(WordPhones);
					QString phonesQ = phonesQRef.toString();

					std::vector<PhoneId> phones;
					if (!parsePhoneList(phoneReg, phonesQ.toStdString(), phones))
					{
						if (errMsg != nullptr) errMsg->utf8Msg = "Can't parse phone list";
						return false;
					}

					PronunciationFlavour pron;
					pron.PronCode = arenaWordRef;
					pron.Phones = std::move(phones);
					wordPron.Pronunciations.push_back(pron);
					singlePronPerWord = true;
				}
			}
			else if (xml.isStartElement() && xml.name() == PronunciationTag)
			{
				if (singlePronPerWord)
				{
					if (errMsg != nullptr) errMsg->utf8Msg = "Word contains phones inside word tag and as child pron tag";
					return false;
				}

				PG_DbgAssert2(!wordPron.Word.empty(), "The word must have been started");

				PronunciationFlavour pron;

				const QXmlStreamAttributes& attrs = xml.attributes();
				if (attrs.hasAttribute(PronunciationCode))
				{
					QStringRef pronCodeStr = attrs.value(PronunciationCode);
					boost::wstring_view pronCodeRef = toWStringRef(pronCodeStr, transientStringBuff);
					pron.PronCode = registerWordThrow2(stringArena, pronCodeRef);
				}
				if (attrs.hasAttribute(WordPhones))
				{
					QStringRef phonesQRef = attrs.value(WordPhones);
					QString phonesQ = phonesQRef.toString();
					
					std::vector<PhoneId> phones;
					if (!parsePhoneList(phoneReg, phonesQ.toStdString(), phones))
					{
						if (errMsg != nullptr) errMsg->utf8Msg = "Can't parse phone list";
						return false;
					}
					pron.Phones = std::move(phones);
				}

				if (pron.PronCode.empty() || pron.Phones.empty())
				{
					if (errMsg != nullptr) errMsg->utf8Msg = "Invalid spec of pronunciation";
					return false;
				}
				wordPron.Pronunciations.push_back(pron);
			}
		}
		maybeFinishPreviousWord(); // finish the last word

		if (xml.hasError()) {
			if (errMsg != nullptr) errMsg->utf8Msg = "Error in XML stucture";
			return false;
		}
		return true;
	}
}
