#include <vector>
#include <QFile>
#include <QXmlStreamReader>
#include <QDomDocument>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "TextProcessing.h"
#include "AppHelpers.h"

namespace PticaGovorunTests
{
	using namespace PticaGovorun;

	struct NumExpansionTestData
	{
		std::wstring Input;
		std::wstring Expect;
	};
	std::ostream& operator<<(std::ostream& os, const NumExpansionTestData& obj)
	{
		std::wstring expectEn;
		transliterateUaToEn(obj.Expect, &expectEn);
		return os << QString::fromStdWString(obj.Input).toStdString() << " "
			<< QString::fromStdWString(expectEn).toStdString();
	}

	struct Int2WordConverterTest : public testing::TestWithParam<NumExpansionTestData>
	{
		IntegerToUaWordConverter int2WordCnv;
	protected:
		void SetUp() override
		{
			auto numDictPath = AppHelpers::mapPath("pgdata/LM_ua/numsCardOrd.xml").toStdWString();
			std::wstring errMsg;
			if (!int2WordCnv.load(numDictPath, &errMsg))
			{
				FAIL() << L"Can't load numbers dictionary. " <<errMsg;
			}
		}
	};

	void parseIntPlusInfo(boost::wstring_view intInfoStr, RawTextLexeme& lex)
	{
		int openInd = (int)intInfoStr.find_first_of(L'(');
		if (openInd == boost::wstring_view::npos)
			return;
		int closeInd = (int)intInfoStr.find_first_of(L')');
		if (closeInd == boost::wstring_view::npos)
			return;

		bool firstWordInit = false;
		int curInd = openInd + 1;
		int commaInd = curInd;
		for(; curInd < closeInd; curInd = commaInd+1)
		{
			commaInd = (int)intInfoStr.find(L',', curInd);
			if (commaInd == (int)boost::wstring_view::npos)
				commaInd = closeInd;

			auto len = commaInd - curInd;
			if (len <= 0) continue;

			boost::wstring_view name = intInfoStr.substr(curInd, len);
			name = trim(name);

			if (!firstWordInit)
			{
				lex.ValueStr = name;
				firstWordInit = true;
				continue;
			}

			if (name == L"N")
				lex.Case = WordCase::Nominative;
			else if (name == L"G")
				lex.Case = WordCase::Genitive;
			else if (name == L"D")
				lex.Case = WordCase::Dative;
			else if (name == L"A")
				lex.Case = WordCase::Acusative;
			else if (name == L"I")
				lex.Case = WordCase::Instrumental;
			else if (name == L"L")
				lex.Case = WordCase::Locative;
			else if (name == L"V")
				lex.Case = WordCase::Vocative;
			
			else if (name == L"card")
				lex.NumeralCardOrd = NumeralCardinality::Cardinal;
			else if (name == L"ord")
				lex.NumeralCardOrd = NumeralCardinality::Ordinal;

			else if (name == L"pl")
				lex.Mulitplicity = EntityMultiplicity::Plural;
			else if (name == L"sing")
				lex.Mulitplicity = EntityMultiplicity::Singular;

			else if (name == L"m")
				lex.setGender(WordGender::Masculine);
			else if (name == L"f")
				lex.setGender(WordGender::Feminine);
			else if (name == L"neut")
				lex.setGender(WordGender::Neuter);
		}
		
	}

	void toStr(gsl::span<RawTextLexeme> sent, std::wstring& str)
	{
		for (int i = 0; i<sent.size(); ++i)
		{
			RawTextLexeme& word = sent[i];
			str.append(word.ValueStr.data(), word.ValueStr.size());

			//if (i + 1 < sent.size())
			//	str.push_back(L' ');
		}
	}

	std::vector<NumExpansionTestData> getNumberExpansionTestData()
	{
		boost::filesystem::path dir = AppHelpers::configParamQString("testDataDir", "testData").toStdWString();
		auto filePath = dir / "text/numberExpansion.xml";
		QFile file(toQString(filePath.wstring()));
		bool openOp = file.open(QIODevice::ReadOnly | QIODevice::Text);
		if (!openOp)
			throw std::runtime_error("Can't open file");

		std::vector<NumExpansionTestData> testDataList;

		QDomDocument doc;
		doc.setContent(&file);
		QDomNodeList testNodes = doc.documentElement().elementsByTagName("t");
		for (int i = 0; i<testNodes.size(); ++i)
		{
			const QDomNode& testNode = testNodes.at(i);
			QString input = testNode.attributes().namedItem("input").nodeValue();
			QString expect = testNode.attributes().namedItem("expect").nodeValue();

			NumExpansionTestData item;
			item.Input = input.toStdWString();
			item.Expect = expect.toStdWString();
			testDataList.push_back(item);
		}
		return testDataList;
	}

	INSTANTIATE_TEST_CASE_P(numExpansionGen, Int2WordConverterTest, testing::ValuesIn(getNumberExpansionTestData()));

	TEST_P(Int2WordConverterTest, expandNumbers)
	{
		const NumExpansionTestData& testData = GetParam();
		RawTextLexeme lex;
		parseIntPlusInfo(testData.Input, lex);

		ptrdiff_t num = boost::lexical_cast<ptrdiff_t>(lex.ValueStr);

		std::vector<RawTextLexeme> sent;
		std::wstring errMsg;
		bool convOp = int2WordCnv.convert(num, lex.NumeralCardOrd.value(), lex.Case.value(),
		                   lex.Mulitplicity, lex.Gender, sent, &errMsg);
		ASSERT_TRUE(convOp) << errMsg;

		std::wstring actualStr;
		toStr(sent, actualStr);

		ASSERT_THAT(testData.Expect, testing::ContainerEq(actualStr));
	}
}
