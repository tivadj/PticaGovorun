#include "stdafx.h"
#include <fstream>
#include <iostream>
#include <string>
//#include <hash_map>
#include <QString>
#include <yaml-cpp/yaml.h>

#include "PticaGovorunCore.h"
#include "LangStat.h"
#include "ClnUtils.h"

namespace
{
	const char* WordName = "word";
	const char* WordUsedCountName = "usedCount";
}

namespace PticaGovorun
{
	//std::tuple<bool, const char*> saveWordUsageInfoYaml(wv::slice<WordSeqUsage*> wordStats, const std::wstring& filePath)
	//{
	//	YAML::Emitter yamlEmit;

	//	std::ostringstream phonesBuf;

	//	yamlEmit << YAML::BeginSeq; // word sequence

	//	QString usedCountStr;
	//	
	//	for (size_t i = 0; i < wordStats.size(); ++i)
	//	{
	//		WordSeqUsage* wordStat = wordStats[i];

	//		QString wordQ = QString::fromWCharArray(wordStat->WordsBuf.data(),wordStat->WordsBuf.size());

	//		yamlEmit << YAML::Flow;
	//		yamlEmit << YAML::BeginMap;
	//		yamlEmit << YAML::Key << WordName;
	//		yamlEmit << YAML::Value << wordQ.toUtf8().constData();
	//		yamlEmit << YAML::Key << WordUsedCountName;

	//		usedCountStr.setNum(wordStat->UsedCount);
	//		yamlEmit << YAML::Value << usedCountStr.toUtf8().constData();
	//		yamlEmit << YAML::EndMap;
	//	}

	//	yamlEmit << YAML::EndSeq; // word sequence

	//	std::ofstream fout(filePath);
	//	fout << yamlEmit.c_str();

	//	return std::make_tuple(true, nullptr);
	//}

	//std::tuple<bool, const char*> loadWordUsageFromYaml(const std::wstring& filePath, std::hash_map<std::wstring, WordSeqUsage>& wordStats, std::vector<WordSeqUsage*>& nodePtrs)
	//{
	//	std::string filePathStd = QString::fromStdWString(filePath).toStdString();

	//	std::vector<std::string> phoneList;

	//	YAML::Node wordRows = YAML::LoadFile(filePathStd);
	//	for (int i = 0; i < wordRows.size(); ++i)
	//	{
	//		YAML::Node wordItemNode = wordRows[i];

	//		std::string word = wordItemNode[WordName].as<std::string>("");
	//		QString wordQ = QString::fromStdString(word);
	//		std::wstring wordW = wordQ.toStdWString();

	//		WordSeqUsage wordNeigh;
	//		wordNeigh.WordsBuf.assign(wordW.begin(), wordW.end());
	//		wordNeigh.WordBounds.push_back(wv::make_view(wordNeigh.WordsBuf));
	//		wordNeigh.UsedCount = wordItemNode[WordUsedCountName].as<decltype(wordNeigh.UsedCount)>();

	//		wordStats.insert(std::make_pair(wordW, std::move(wordNeigh)));
	//		nodePtrs.push_back(&wordStats.at(wordW));
	//	}

	//	return std::make_pair(true, nullptr);
	//}
}
