#include "stdafx.h"
#include "PhoneticService.h"

#include <fstream>

#include <yaml-cpp/yaml.h>
#include "ClnUtils.h"

namespace
{
	const char* WordName = "word";
	const char* WordUsedCountName = "phones";
	const char* PronunciationListName = "prons";
	const char* PronunciationAsWordName = "pronId";
}

namespace PticaGovorun
{
	void savePhoneticDictionaryYaml(const std::vector<PhoneticWord>& phoneticDict, const std::wstring& filePath, const PhoneRegistry& phoneReg)
	{
		YAML::Emitter yamlEmit;

		std::string phoneListStr;

		yamlEmit << YAML::BeginSeq; // word sequence

		for (const PhoneticWord& wordAndProns : phoneticDict)
		{
			QString wordQ = QString::fromStdWString(wordAndProns.Word);

			// word and single pronunciation can be collapsed into single line
			// if pronAsWord is equal to word itself

			if (wordAndProns.Pronunciations.size() == 1 && wordAndProns.Word == wordAndProns.Pronunciations[0].PronAsWord)
			{
				yamlEmit << YAML::Flow;
				yamlEmit << YAML::BeginMap;
				yamlEmit << YAML::Key << WordName;
				yamlEmit << YAML::Value << wordQ.toUtf8().constData();
				yamlEmit << YAML::Key << WordUsedCountName;

				phoneListStr.clear();
				phoneListToStr(phoneReg, wordAndProns.Pronunciations[0].Phones, phoneListStr);

				yamlEmit << YAML::Value << phoneListStr;
				yamlEmit << YAML::EndMap;
				continue;
			}

			//
			yamlEmit << YAML::BeginMap;
			{
				yamlEmit << YAML::Key << WordName;
				yamlEmit << YAML::Value << wordQ.toUtf8().constData();

				yamlEmit << YAML::Key << PronunciationListName;
				yamlEmit << YAML::Value;

				// write all pronunciations
				yamlEmit << YAML::BeginSeq;
				{
					for (size_t pronInd = 0; pronInd < wordAndProns.Pronunciations.size(); ++pronInd)
					{
						const PronunciationFlavour& pron = wordAndProns.Pronunciations[pronInd];
						yamlEmit << YAML::Flow;
						yamlEmit << YAML::BeginMap;
						yamlEmit << YAML::Key << PronunciationAsWordName;
						yamlEmit << YAML::Value << QString::fromStdWString(pron.PronAsWord).toUtf8().constData();

						phoneListStr.clear();
						phoneListToStr(phoneReg, pron.Phones, phoneListStr);

						yamlEmit << YAML::Key << WordUsedCountName;
						yamlEmit << YAML::Value << phoneListStr;
						yamlEmit << YAML::EndMap;
					}
				}
				yamlEmit << YAML::EndSeq; // word sequence
			}
			yamlEmit << YAML::EndMap; // word map
		}

		yamlEmit << YAML::EndSeq; // word sequence

		std::ofstream fout(filePath);
		fout << yamlEmit.c_str();
	}

	void savePhoneticDictionaryYamlNew(const std::vector<PhoneticWordNew>& phoneticDict, const std::wstring& filePath, const PhoneRegistry& phoneReg)
	{
		YAML::Emitter yamlEmit;

		std::string phoneListStr;

		yamlEmit << YAML::BeginSeq; // word sequence

		for (const PhoneticWordNew& wordAndProns : phoneticDict)
		{
			QString wordQ = toQString(wordAndProns.Word);

			// word and single pronunciation can be collapsed into single line
			// if pronAsWord is equal to word itself

			if (wordAndProns.Pronunciations.size() == 1 && wordAndProns.Word == wordAndProns.Pronunciations[0].PronCode)
			{
				yamlEmit << YAML::Flow;
				yamlEmit << YAML::BeginMap;
				yamlEmit << YAML::Key << WordName;
				yamlEmit << YAML::Value << wordQ.toUtf8().constData();
				yamlEmit << YAML::Key << WordUsedCountName;

				phoneListStr.clear();
				phoneListToStr(phoneReg, wordAndProns.Pronunciations[0].Phones, phoneListStr);

				yamlEmit << YAML::Value << phoneListStr;
				yamlEmit << YAML::EndMap;
				continue;
			}

			//
			yamlEmit << YAML::BeginMap;
			{
				yamlEmit << YAML::Key << WordName;
				yamlEmit << YAML::Value << wordQ.toUtf8().constData();

				yamlEmit << YAML::Key << PronunciationListName;
				yamlEmit << YAML::Value;

				// write all pronunciations
				yamlEmit << YAML::BeginSeq;
				{
					for (size_t pronInd = 0; pronInd < wordAndProns.Pronunciations.size(); ++pronInd)
					{
						const PronunciationFlavourNew& pron = wordAndProns.Pronunciations[pronInd];
						yamlEmit << YAML::Flow;
						yamlEmit << YAML::BeginMap;
						yamlEmit << YAML::Key << PronunciationAsWordName;
						yamlEmit << YAML::Value << toQString(pron.PronCode).toUtf8().constData();

						phoneListStr.clear();
						phoneListToStr(phoneReg, pron.Phones, phoneListStr);

						yamlEmit << YAML::Key << WordUsedCountName;
						yamlEmit << YAML::Value << phoneListStr;
						yamlEmit << YAML::EndMap;
					}
				}
				yamlEmit << YAML::EndSeq; // word sequence
			}
			yamlEmit << YAML::EndMap; // word map
		}

		yamlEmit << YAML::EndSeq; // word sequence

		std::ofstream fout(filePath);
		fout << yamlEmit.c_str();
	}

	std::tuple<bool, const char*> loadPhoneticDictionaryYaml(const std::wstring& filePath, const PhoneRegistry& phoneReg, std::vector<PhoneticWord>& phoneticDict)
	{
		std::string filePathStd = QString::fromStdWString(filePath).toStdString();

		std::vector<PhoneId> phoneList;
		
		YAML::Node wordRows = YAML::LoadFile(filePathStd);
		for (int i = 0; i < wordRows.size(); ++i)
		{
			YAML::Node wordItemNode = wordRows[i];

			PhoneticWord wordPhoneticInfo;

			std::string word = wordItemNode[WordName].as<std::string>("");
			QString wordQ = QString::fromStdString(word);
			std::wstring wordW = wordQ.toStdWString();
			wordPhoneticInfo.Word = wordW;

			//
			YAML::Node phonesNode = wordItemNode[WordUsedCountName];
			YAML::Node pronsNode = wordItemNode[PronunciationListName];
			bool isOne = (bool)phonesNode ^ (bool)pronsNode;
			if (!isOne)
				return std::make_pair(false, "Exactly one must be defined");

			auto pushWordPron = [&phoneList, &wordPhoneticInfo, &phoneReg](const std::wstring& pronAsWord, const std::string& phoneListStrs) -> std::tuple<bool, const char*>
			{
				phoneList.clear();
				bool parsePhoneOp = parsePhoneList(phoneReg, phoneListStrs, phoneList);
				if (!parsePhoneOp)
					return std::make_pair(false, "Can't parse phone string");

				PronunciationFlavour pron;
				pron.PronAsWord = pronAsWord;
				pron.Phones = phoneList;
				wordPhoneticInfo.Pronunciations.push_back(pron);
				return std::make_pair(true, nullptr);
			};

			std::tuple<bool, const char*> parseOp;
			if (phonesNode)
			{
				std::string phones = phonesNode.as<std::string>();

				parseOp = pushWordPron(wordW, phones);
				if (!std::get<0>(parseOp))
					return parseOp;
			}
			else
			{
				for (int pronInd = 0; pronInd < pronsNode.size(); ++pronInd)
				{
					std::string pronId = pronsNode[pronInd][PronunciationAsWordName].as<std::string>("");
					QString pronIdQ = QString::fromStdString(pronId);

					std::string phones = pronsNode[pronInd][WordUsedCountName].as<std::string>("");
					
					parseOp = pushWordPron(pronIdQ.toStdWString(), phones);
					if (!std::get<0>(parseOp))
						return parseOp;
				}
			}
			phoneticDict.push_back(wordPhoneticInfo);
		}

		return std::make_pair(true, nullptr);
	}

	std::tuple<bool, const char*> loadPhoneticDictionaryYamlNew(const std::wstring& filePath, const PhoneRegistry& phoneReg, std::vector<PhoneticWordNew>& phoneticDict, GrowOnlyPinArena<wchar_t>& stringArena)
	{
		std::string filePathStd = QString::fromStdWString(filePath).toStdString();

		std::vector<PhoneId> phoneList;
		
		YAML::Node wordRows = YAML::LoadFile(filePathStd);
		for (int i = 0; i < wordRows.size(); ++i)
		{
			YAML::Node wordItemNode = wordRows[i];

			PhoneticWordNew wordPhoneticInfo;

			std::string word = wordItemNode[WordName].as<std::string>("");
			QString wordQ = QString::fromStdString(word);
			std::wstring wordW = wordQ.toStdWString();

			boost::wstring_ref arenaWordRef;
			if (!registerWord(static_cast<boost::wstring_ref>(wordW), stringArena, arenaWordRef))
				return std::make_pair(false, "Can't addd word to arena");

			wordPhoneticInfo.Word = arenaWordRef;

			//
			YAML::Node phonesNode = wordItemNode[WordUsedCountName];
			YAML::Node pronsNode = wordItemNode[PronunciationListName];
			bool isOne = (bool)phonesNode ^ (bool)pronsNode;
			if (!isOne)
				return std::make_pair(false, "Exactly one must be defined");

			auto pushWordPron = [&phoneList, &wordPhoneticInfo, &phoneReg](boost::wstring_ref pronCode, boost::string_ref& phoneListStrs) -> std::tuple<bool, const char*>
			{
				phoneList.clear();
				bool parsePhoneOp = parsePhoneList(phoneReg, phoneListStrs, phoneList);
				if (!parsePhoneOp)
					return std::make_pair(false, "Can't parse phone string");

				PronunciationFlavourNew pron;
				pron.PronCode = pronCode;
				pron.Phones = phoneList;
				wordPhoneticInfo.Pronunciations.push_back(pron);
				return std::make_pair(true, nullptr);
			};

			std::tuple<bool, const char*> parseOp;
			if (phonesNode)
			{
				std::string phones = phonesNode.as<std::string>();

				parseOp = pushWordPron(arenaWordRef, static_cast<boost::string_ref>(phones));
				if (!std::get<0>(parseOp))
					return parseOp;
			}
			else
			{
				for (size_t pronInd = 0; pronInd < pronsNode.size(); ++pronInd)
				{
					std::string pronId = pronsNode[pronInd][PronunciationAsWordName].as<std::string>("");
					QString pronIdQ = QString::fromStdString(pronId);

					std::string phones = pronsNode[pronInd][WordUsedCountName].as<std::string>("");
					
					boost::wstring_ref arenaPronCodeRef;
					if (!registerWord(static_cast<boost::wstring_ref>(pronIdQ.toStdWString()), stringArena, arenaPronCodeRef))
						return std::make_pair(false, "Can't addd word to arena");

					parseOp = pushWordPron(arenaPronCodeRef, static_cast<boost::string_ref>(phones));
					if (!std::get<0>(parseOp))
						return parseOp;
				}
			}
			phoneticDict.push_back(wordPhoneticInfo);
		}

		return std::make_pair(true, nullptr);
	}
}