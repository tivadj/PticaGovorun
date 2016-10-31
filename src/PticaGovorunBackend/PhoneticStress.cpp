#include "PhoneticService.h"
#include <unordered_map>
#include <QFile>
#include <QXmlStreamReader>
#include <QString>
#include <boost/format.hpp>
#include "assertImpl.h"

namespace PticaGovorun
{
	// Gets the index of unique stressed vowel.
	// If there are more than one vowel, the function return false.
	// If there is no vowels, the result index is initialized to -1 and function returns true.
	bool getStressedVowelCharIndAtMostOne(boost::wstring_view word, int& stressedCharInd)
	{
		stressedCharInd = -1;

		int numVowels = 0;
		for (size_t i = 0; i < word.size(); ++i)
		{
			wchar_t ch = word[i];
			if (isUkrainianVowel(ch))
			{
				numVowels++;
				stressedCharInd = (int)i;
			}
		}
		if (numVowels == 0)
		{
			// word may have no vowels (eg some abbreviation)
			stressedCharInd = -1;
			return true;
		}
		else if (numVowels == 1)
		{
			// one vowel in a word is always stressed (not works with abbreviation eg USA)
			PG_DbgAssert2(stressedCharInd != -1, "The result index is already initialized");
			return true;
		}
		return false;
	}

	// Find the char index of the vowel corresonding to provided syllable.
	// returns -1 if there are not enough vowels in the word.
	int syllableIndToVowelCharIndUk(boost::wstring_view word, int syllableInd)
	{
		int numVowels = 0;
		for (int i = 0; i < (int)word.size(); ++i)
		{
			if (isUkrainianVowel(word[i]))
			{
				if (numVowels == syllableInd)
					return i;

				numVowels++;
			}
		}
		return -1;
	}

	// Returns the index of refered by char index vowel in the array of all vowels of the word.
	// eg. for word 'auntie' char indices [0 1 4 5] would map to [0 1 2 3].
	// returns -1 if input char is not a vowel.
	int vowelCharIndToSyllableIndUk(boost::wstring_view word, int vowelCharInd)
	{
		wchar_t letter = word[vowelCharInd];
		bool isVowel = isUkrainianVowel(letter);
		if (!isVowel)
			return -1; // The index of input char must be vowel

		int numVowels = 0;
		for (int i = 0; i < vowelCharInd && i < (int)word.size(); ++i)
		{
			if (isUkrainianVowel(word[i]))
				numVowels++;
		}
		return numVowels;
	}

	namespace
	{
		const char* SressDictWordTag = "word";
		const char* SressDictWordName = "name";
		const char* SressDictWordSyllable = "stressedSyllable";
	}

	bool loadStressedSyllableDictionaryXml(const boost::filesystem::path& dictFilePath, std::unordered_map<std::wstring, int>& wordToStressedSyllableInd, ErrMsgList* errMsg)
	{
		auto readStream = [&]()
		{
			QFile file(toQStringBfs(dictFilePath));
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				//pushErrorMsg(errMsg, str(boost::format("Can't open file for reading (%1%)") % dictFilePath.string()));
				pushErrorMsg(errMsg, "Can't open file");
				return false;
			}
			QXmlStreamReader xmlReader(&file);
			std::wstring wordName;
			while (!xmlReader.atEnd())
			{
				xmlReader.readNext();
				if (xmlReader.isStartElement() && xmlReader.name() == SressDictWordTag)
				{
					wordName.clear();
					int wordStressedSyllableInd = -1;

					const QXmlStreamAttributes& attrs = xmlReader.attributes();
					if (attrs.hasAttribute(SressDictWordName))
					{
						QStringRef nameStr = attrs.value(SressDictWordName);
						QString nameQ = QString::fromRawData(nameStr.constData(), nameStr.size());
						wordName = nameQ.toStdWString();
					}
					if (attrs.hasAttribute(SressDictWordSyllable))
					{
						QStringRef stressStr = attrs.value(SressDictWordSyllable);
						int sepInd = stressStr.indexOf(' ');
						if (sepInd != -1)
							stressStr = stressStr.left(sepInd);

						bool convOp = false;
						wordStressedSyllableInd = stressStr.toInt(&convOp);
						if (!convOp)
						{
							pushErrorMsg(errMsg, str(boost::format("Can't parse stressed syllable definition for word %1%") % toUtf8StdString(wordName)));
							return false;
						}

						wordStressedSyllableInd -= 1; // dict has 1-based index
					}

					if (wordName.empty() || wordStressedSyllableInd == -1)
					{
						pushErrorMsg(errMsg, str(boost::format("The stress definition is incomplete for word %1%") % toUtf8StdString(wordName)));
						return false;
					}

					auto checkSyllableInd = [](const std::wstring& word, int syllabInd)
					{
						if (syllabInd < 0)
							return false;

						int vowelsCount = vowelsCountUk(word);
						if (syllabInd >= vowelsCount)
							return false;
						return true;
					};
					if (!checkSyllableInd(wordName, wordStressedSyllableInd))
					{
						pushErrorMsg(errMsg, "Syllable index is out of range");
						return false;
					}

					wordToStressedSyllableInd[wordName] = wordStressedSyllableInd;
				}

			}
			if (xmlReader.hasError())
			{
				pushErrorMsg(errMsg, std::string("XmlReader error: ") + toUtf8StdString(xmlReader.errorString()));
				return false;
			}
			return true;
		};
		if (!readStream())
		{
			pushErrorMsg(errMsg, str(boost::format("Can't read stressed syllables data from xml file (%1%)") % dictFilePath.string()));
			return false;
		}
		return true;
	}
}