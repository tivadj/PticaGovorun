#include <vector>
#include <gsl/span>
#include <QXmlStreamReader>
#include <QTextStream>
#include <QFile>
#include <QDomDocument>
#include <boost/filesystem/path.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "TextProcessing.h"
#include "AppHelpers.h"

namespace PticaGovorunTests
{
	using namespace PticaGovorun;

	struct DummyTextBlockReader : TextBlockReader
	{
		std::vector<std::wstring> textBlocks_;
		int textBlockInd_ = 0;

		DummyTextBlockReader(std::initializer_list<std::wstring> blocks)
		{
			textBlocks_ = blocks;
		}

		bool nextBlock(std::vector<wchar_t>& buff) override
		{
			if (textBlockInd_ >= textBlocks_.size())
				return false;
			auto& str = textBlocks_[textBlockInd_];

			buff.resize(str.size());
			str.copy(buff.data(), buff.size(), 0);

			textBlockInd_ += 1;
			return true;
		}
	};

	struct TextParsingTestData
	{
		std::wstring Input;
		std::wstring Expect;
	};
	std::ostream& operator<<(std::ostream& os, const TextParsingTestData& obj)
	{
		std::wstring inputEn;
		transliterateUaToEn(obj.Input, &inputEn);
		return os << QString::fromStdWString(inputEn).toStdString();
	}

	struct TextParseSentenceTest : public testing::TestWithParam<TextParsingTestData>
	{
		std::unique_ptr<SentenceParser> sentParser;
		std::shared_ptr<GrowOnlyPinArena<wchar_t>> arena;
	protected:
		void SetUp() override
		{
			arena = std::make_unique<GrowOnlyPinArena<wchar_t>>(1024);
			sentParser = std::make_unique<SentenceParser>(1024);

			auto abbrExp = std::make_shared<AbbreviationExpanderUkr>();
			abbrExp->stringArena_ = arena;

			auto numDictPath = AppHelpers::mapPath("pgdata/LM_ua/numsCardOrd.xml").toStdWString();
			std::wstring errMsg;
			if (!abbrExp->load(numDictPath, &errMsg))
			{
				FAIL() << L"Can't load numbers dictionary. " << errMsg;
			}
			sentParser->setAbbrevExpander(abbrExp);
		}

	public:
		void getAllSent(TextBlockReader* textReader, std::vector<std::vector<RawTextLexeme>>& allSent)
		{
			sentParser->setTextBlockReader(textReader);
			sentParser->setOnNextSentence([&](gsl::span<const RawTextLexeme>& sent)
				{
					std::vector<RawTextLexeme> oneSent;
					registerInArena(*arena, sent, oneSent);
					removeWhitespaceLexemes(oneSent);
					allSent.push_back(std::move(oneSent));
				});
			sentParser->run();
		}
	};

	auto str(const RawTextLexeme& x) -> auto { return x.ValueStr; }

	void allSentToStr(const std::vector<std::vector<RawTextLexeme>>& allSent, std::wstring& str)
	{
		static const auto SentSep = L" | ";
		static const auto WordSep = L" ";

		for (int sentInd=0; sentInd < allSent.size(); ++sentInd)
		{
			auto& sent = allSent[sentInd];
			for (int wordInd = 0; wordInd < sent.size(); ++wordInd)
			{
				auto& lex = sent[wordInd];
				str.append(lex.ValueStr.data(), lex.ValueStr.size());
				
				if (wordInd + 1 < sent.size())
					str.append(WordSep);
			}
			if (sentInd + 1 < allSent.size())
			{
				str.append(SentSep);
			}
		}
	}

	std::vector<TextParsingTestData> getParseSentenceTestData()
	{
		boost::filesystem::path dir = AppHelpers::configParamQString("testDataDir", "testData").toStdWString();
		auto filePath = dir / "text/parseSentence.xml";
		QFile file(toQString(filePath.wstring()));
		bool openOp = file.open(QIODevice::ReadOnly | QIODevice::Text);
		if (!openOp)
			throw std::runtime_error("Can't open file");

		std::vector<TextParsingTestData> testDataList;

		QDomDocument doc;
		doc.setContent(&file);
		QDomNodeList testNodes = doc.documentElement().elementsByTagName("t");
		for (int i = 0; i<testNodes.size(); ++i)
		{
			const QDomNode& testNode = testNodes.at(i);
			QString input = testNode.attributes().namedItem("input").nodeValue();
			QString expect = testNode.attributes().namedItem("expect").nodeValue();

			TextParsingTestData n;
			n.Input = input.toStdWString();
			n.Expect = expect.toStdWString();
			testDataList.push_back(n);
		}

		return testDataList;
	}

	INSTANTIATE_TEST_CASE_P(parseSentenceGen, TextParseSentenceTest, testing::ValuesIn(getParseSentenceTestData()));

	TEST_P(TextParseSentenceTest, parseSentence)
	{
		const TextParsingTestData& testData = GetParam();
		DummyTextBlockReader text({ testData.Input });

		std::vector<std::vector<RawTextLexeme>> allSent;
		getAllSent(&text, allSent);

		std::wstring allSentStr;
		allSentToStr(allSent, allSentStr);


		EXPECT_THAT(testData.Expect, testing::ContainerEq(allSentStr));
	}
}
