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
	void savePhoneticDictionaryYaml(const std::vector<PhoneticWord>& phoneticDict, const std::wstring& filePath)
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
				phoneListToStr(wordAndProns.Pronunciations[0].PhoneIds, phoneListStr);

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
						phoneListToStr(pron.PhoneIds, phoneListStr);

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

	std::tuple<bool, const char*> loadPhoneticDictionaryYaml(const std::wstring& filePath, std::vector<PhoneticWord>& phoneticDict)
	{
		std::string filePathStd = QString::fromStdWString(filePath).toStdString();

		std::vector<UkrainianPhoneId> phoneList;
		
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

			auto pushWordPron = [&phoneList, &wordPhoneticInfo](const std::wstring& pronAsWord, const std::string& phoneListStrs) -> std::tuple<bool, const char*>
			{
				phoneList.clear();
				bool parsePhoneOp = parsePhoneListStrs(phoneListStrs, phoneList);
				if (!parsePhoneOp)
					return std::make_pair(false, "Can't parse phone string");

				PronunciationFlavour pron;
				pron.PronAsWord = pronAsWord;
				pron.PhoneIds = phoneList;
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
}