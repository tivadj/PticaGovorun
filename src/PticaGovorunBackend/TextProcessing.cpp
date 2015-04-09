#include "stdafx.h"
#include "TextProcessing.h"
#include <QChar>

namespace PticaGovorun
{
	void TextParser::setInputText(wv::slice<wchar_t> text)
	{
		mutText_ = text;
		charInd_ = 0;
	}

	bool TextParser::parseSentence(std::vector<wv::slice<wchar_t>>& words)
	{
		// entire buffer is processed, no sentences left
		if (charInd_ >= mutText_.size())
			return false;

		wordStartInd_ = -1; // start of new word in input stream of characters
		outCharInd_ = -1; // the chars are processed without any shifts
		gotApostrophe_ = false;
		gotHyphen_ = false;

		for (; charInd_ < mutText_.size(); ++charInd_)
		{
			wchar_t chw = mutText_[charInd_];
			switch (chw)
			{
			case L'.':
			case L'?':
			case L'!':
			case L'…': // horizontal ellipsis code=8230
			{
				// finish sentence
				if (isWordStarted())
				{
					finishWord(words);
				}

				// +1 to step behind the sentence break character
				charInd_ += 1; // next unprocessed char
				return true;
			}
			case L'\t':
			case L'\r':
			case L'\n':
			case L' ': // space
			case L' ': // dec=160 no-break space (weird case after long hyphen, not a space)
			case L'_': // dec=31  us information separator one (underscore)
			case L'\"': // dec=34 quotation mark
			case L'#': // dec=35 number sign
			case L'$': // dec=36 dollar sign
			case L'%': // dec=37 percent sign
			case L'&': // dec=38 ampersand
			case L'+': // dec=43 plus sign
			case L'/': // dec=47 solidus
			case L':': // dec=58 colon
			case L';': // dec=59 semicolon
			case L'<': // dec=60 less-than sign
			case L'=': // dec=61 equals sign
			case L'>': // dec=62 greater-than sign
			case L'@': // dec=64 commercial at
			case L'\\': // dec=92 reverse solidus (backslash)
			case L'^': // dec=94 circumflex accent
			case L'[': // dec=91 left square bracket
			case L']':
			case L'(':
			case L')':
			case L'{': // dec=123 left curly bracket
			case L'}':
			case L',': // dec=44 comma
			case L'‚': // dec=8218 single low-9 quotation mark
			case L'§': // dec=167 section sign
			case L'©': // dec=169 copyright sign
			case L'«': // dec=171 left-pointing double angle quotation mark
			case L'»': // dec=187 right-pointing double angle quotation mark
			case L'“': // dec=8220 left double quotation mark
			case L'”': // right double quotation mark
			case L'„': // dec=8222 double low-9 quotation mark
			case L'‟': // dec=8223 double high-reversed-9 quotation mark
			case L'–': // dec=8211 en dash (slightly longer than    dash)
			case L'—': // dec=8212 em dash (slightly longer than en dash)
			case L'*':
			case L'•': // dec=8226 bullet
			case L'№': // dec=8470 numero sign
			case L'': // dec=61449 ? (used inside xml's emphasis tag)
				onWordBreak(chw, words);
				break;
			case L'¬': // not sign (optional hyphen)
				// the word should be merged, as if this mark doesn't exist
				if (isWordStarted()) // if the word already started
				{
					outCharInd_ = charInd_;
				}
				break;

			case L'-':
				// if the dash is inside the word, then we have a compounded word
				// otherwise it is a word separator
				if (isWordStarted())
				{
					onWordCharacter(chw);
					gotHyphen_ = true;
				}
				else
					onWordBreak(chw, words);
				break;

			case L'\'': // apostrophe dec=39
			case L'’': // right single quotation mark dec=8217
			case L'`': // grave accent (on the tilde key) dec=96
				// apostrophe at the word boundary is treated as a word separator
				if (isWordStarted())
				{
					// character of a word
					onWordCharacter(chw);
					gotApostrophe_ = true;
				}
				else
					onWordBreak(chw, words);
				break;
			default:
				// character of a word
				onWordCharacter(chw);
				gotApostrophe_ = false;
				gotHyphen_ = false;
				break;
			}
		}

		// finish the last word
		if (wordStartInd_ != -1)
			finishWord(words);

		return true;
	}

	void TextParser::finishWord(std::vector<wv::slice<wchar_t>>& words)
	{
		int wordEndIndExcl;
		if (outCharInd_ != -1) // word shifting state
			wordEndIndExcl = outCharInd_;
		else
			wordEndIndExcl = charInd_;

		// when word ends with apostrophe or hyphen, then treat apostrophe or hyphen as a separator
		if (gotApostrophe_ || gotHyphen_)
			wordEndIndExcl -= 1;

		PG_Assert(wordStartInd_ < wordEndIndExcl && "Word has at least one character");

		int len = wordEndIndExcl - wordStartInd_;
		wv::slice<wchar_t> wordSlice = wv::make_view(&mutText_[wordStartInd_], len);
		words.push_back(wordSlice);
#if PG_DEBUG
		std::wstring tmp = toString(wordSlice);
#endif
		// prepare for new word
		wordStartInd_ = -1;
		outCharInd_ = -1;
		gotApostrophe_ = false;
		gotHyphen_ = false;
	}

	void TextParser::onWordCharacter(wchar_t chw)
	{
		if (wordStartInd_ == -1)
		{
			wordStartInd_ = charInd_;
		}
		else
		{
			// continue word
		}

		wchar_t outChar = chw;
		if (chw == L'’' || chw == L'`')
		{
			// normalize various ways to represent apostrophe
			outChar = L'\'';
		}
		else
		{
			// normalize, so that all words are in lower case
			QChar qChar(chw);
			if (!qChar.isLower())
			{
				outChar = qChar.toLower().unicode();
			}
		}

		// x the character is overwritten if it is uppercase and need to be converted in a lowercase
		// x there was an optional hypen (¬) in the word and characters after the optional hyphen must be shifted one symbol to the left
		
		if (outChar != chw ||
			outCharInd_ != -1) // shifting chars
		{
			int putPos;
			if (outCharInd_ != -1)
			{
				putPos = outCharInd_;
				outCharInd_++;
			}
			else
				putPos = charInd_;
			mutText_[putPos] = outChar;
		}
	}

	bool TextParser::isWordStarted() const
	{
		return wordStartInd_ != -1;
	}

	void TextParser::onWordBreak(wchar_t chw, std::vector<wv::slice<wchar_t>>& words)
	{
		if (wordStartInd_ == -1)
		{
			// empty space before any word, continue
		}
		else
		{
			PG_Assert(wordStartInd_ >= 0);
			PG_Assert(wordStartInd_ < charInd_);
			finishWord(words);
		}
	}

	//

	std::wstring toString(wv::slice<wchar_t> wordSlice)
	{
		return std::wstring(wordSlice.data(), wordSlice.size());
	}

	bool isDigitChar(wchar_t ch)
	{
		int chCode = (int)ch;
		bool result = chCode >= (int)L'0' && chCode <= (int)L'9';
		return result;
	}

	bool isEnglishChar(wchar_t ch)
	{
		int chCode = (int)ch;
		bool isLarge = chCode >= L'A' && chCode <= L'Z';
		bool isSmall = chCode >= L'a' && chCode <= L'z';
		return isSmall || isLarge;
	}

	bool isExclusiveEnglishChar(wchar_t ch)
	{
		return 
			ch == L'b' ||
			ch == L'd' || ch == L'D' ||
			ch == L'f' || ch == L'F' ||
			ch == L'g' || ch == L'G' ||
			ch == L'h' ||
			ch == L'j' || ch == L'J' ||
			ch == L'k' ||
			ch == L'l' || ch == L'L' ||
			ch == L'm' ||
			ch == L'n' || ch == L'N' ||
			ch == L'q' || ch == L'Q' ||
			ch == L'r' || ch == L'R' ||
			ch == L's' || ch == L'S' ||
			ch == L't' ||
			ch == L'u' || ch == L'U' ||
			ch == L'v' || ch == L'V' ||
			ch == L'w' || ch == L'W' ||
			ch == L'y' || ch == L'Y' ||
			ch == L'z' || ch == L'Z';
	}

	bool isRussianChar(wchar_t ch)
	{
		// абвгдеёжзийклмнопрстуфхцчшщъыьэюя
		// АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ
		return
			ch == L'а' || ch == L'А' ||
			ch == L'б' || ch == L'Б' ||
			ch == L'в' || ch == L'В' ||
			ch == L'г' || ch == L'Г' ||
			ch == L'д' || ch == L'Д' ||
			ch == L'е' || ch == L'Е' ||
			ch == L'ё' || ch == L'Ё' ||
			ch == L'ж' || ch == L'Ж' ||
			ch == L'з' || ch == L'З' ||
			ch == L'и' || ch == L'И' ||
			ch == L'й' || ch == L'Й' ||
			ch == L'к' || ch == L'К' ||
			ch == L'л' || ch == L'Л' ||
			ch == L'м' || ch == L'М' ||
			ch == L'н' || ch == L'Н' ||
			ch == L'о' || ch == L'О' ||
			ch == L'п' || ch == L'П' ||
			ch == L'р' || ch == L'Р' ||
			ch == L'с' || ch == L'С' ||
			ch == L'т' || ch == L'Т' ||
			ch == L'у' || ch == L'У' ||
			ch == L'ф' || ch == L'Ф' ||
			ch == L'х' || ch == L'Х' ||
			ch == L'ц' || ch == L'Ц' ||
			ch == L'ч' || ch == L'Ч' ||
			ch == L'ш' || ch == L'Ш' ||
			ch == L'щ' || ch == L'Щ' ||
			ch == L'ъ' || ch == L'Ъ' ||
			ch == L'ы' || ch == L'Ы' ||
			ch == L'ь' || ch == L'Ь' ||
			ch == L'э' || ch == L'Э' ||
			ch == L'ю' || ch == L'Ю' ||
			ch == L'я' || ch == L'Я';
	}

	bool isExclusiveRussianChar(wchar_t ch)
	{
		return
			ch == L'ё' || ch == L'Ё' ||
			ch == L'ъ' || ch == L'Ъ' ||
			ch == L'ы' || ch == L'Ы' ||
			ch == L'э' || ch == L'Э';
	}

	bool isUkrainianConsonant(wchar_t ch)
	{
		// бвгґджзйклмнпрстфхцчшщ
		// БВГҐДЖЗЙКЛМНПРСТФХЦЧШЩ
		return 
			ch == L'б' || ch == L'Б' ||
			ch == L'в' || ch == L'В' ||
			ch == L'г' || ch == L'Г' ||
			ch == L'ґ' || ch == L'Ґ' ||
			ch == L'д' || ch == L'Д' ||
			ch == L'ж' || ch == L'Ж' ||
			ch == L'з' || ch == L'З' ||
			ch == L'й' || ch == L'Й' ||
			ch == L'к' || ch == L'К' ||
			ch == L'л' || ch == L'Л' ||
			ch == L'м' || ch == L'М' ||
			ch == L'н' || ch == L'Н' ||
			ch == L'п' || ch == L'П' ||
			ch == L'р' || ch == L'Р' ||
			ch == L'с' || ch == L'С' ||
			ch == L'т' || ch == L'Т' ||
			ch == L'ф' || ch == L'Ф' ||
			ch == L'х' || ch == L'Х' ||
			ch == L'ц' || ch == L'Ц' ||
			ch == L'ч' || ch == L'Ч' ||
			ch == L'ш' || ch == L'Ш' ||
			ch == L'щ' || ch == L'Щ';
	}

	bool isUkrainianVowel(wchar_t ch)
	{
		// аеєиіїоуюя
		// АЕЄИІЇОУЮЯ
		return
			ch == L'а' || ch == L'А' ||
			ch == L'е' || ch == L'Е' ||
			ch == L'є' || ch == L'Є' ||
			ch == L'и' || ch == L'И' ||
			ch == L'і' || ch == L'І' ||
			ch == L'ї' || ch == L'Ї' ||
			ch == L'о' || ch == L'О' ||
			ch == L'у' || ch == L'У' ||
			ch == L'ю' || ch == L'Ю' ||
			ch == L'я' || ch == L'Я';
	}
}