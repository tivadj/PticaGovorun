#include "stdafx.h"
#include "PhoneticService.h"
#include <unordered_map>
#include <QFile>
#include <QXmlStreamReader>
#include <QString>
#include "assertImpl.h"

namespace PticaGovorun
{
	// Gets the index of unique stressed vowel.
	// If there are more than one vowel, the function return false.
	// If there is no vowels, the result index is initialized to -1 and function returns true.
	bool getStressedVowelCharIndAtMostOne(boost::wstring_ref word, int& stressedCharInd)
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
	int syllableIndToVowelCharIndUk(boost::wstring_ref word, int syllableInd)
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
	int vowelCharIndToSyllableIndUk(boost::wstring_ref word, int vowelCharInd)
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

	std::tuple<bool, const char*> loadStressedSyllableDictionaryXml(boost::wstring_ref dictFilePath, std::unordered_map<std::wstring, int>& wordToStressedSyllableInd)
	{
		QFile file(QString::fromWCharArray(dictFilePath.begin(), dictFilePath.size()));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file");

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
						return std::make_tuple(false, "Can't parse stressed syllable definition");
					
					wordStressedSyllableInd -= 1; // dict has 1-based index
				}

				if (wordName.empty() || wordStressedSyllableInd == -1)
					return std::make_tuple(false, "The word stress definition is incomplete");

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
					std::make_tuple(false, "Syllable index is out of range");

				wordToStressedSyllableInd[wordName] = wordStressedSyllableInd;
			}
		}
		if (xmlReader.hasError())
			return std::make_tuple(false, "Error reading XML stressed syllables definitions");

		return std::make_tuple(true, nullptr);
	}
}