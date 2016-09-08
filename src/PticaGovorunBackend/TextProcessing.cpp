﻿#include "stdafx.h"
#include "TextProcessing.h"
#include "assertImpl.h"
#include <QChar>
#include <cwchar>
#include <gsl/span>
#include <boost/lexical_cast.hpp>

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

		PG_Assert2(wordStartInd_ < wordEndIndExcl, "Word has at least one character");

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

	void TextParserNew::setInputText(boost::wstring_view text)
	{
		text_ = text;
		curCharInd_ = 0;
	}

	void TextParserNew::setTextRunDest(std::vector<RawTextRun>* textRunDest)
	{
		textRunDest_ = textRunDest;
	}

	bool TextParserNew::parseTokensTillDot(std::vector<RawTextRun>& tokens)
	{
		PG_Assert2(curCharInd_ != -1, "Text buffer to read was not initialized. Call setInputText");
		textRunDest_ = &tokens;

		// entire buffer is processed, no sentences left
		if (curCharInd_ >= text_.size())
			return false;

		textRunStartInd_ = -1; // start of new word in input stream of characters
		//outCharInd_ = -1; // the chars are processed without any shifts
		//gotApostrophe_ = false;
		//gotHyphen_ = false;

		for (; curCharInd_ < text_.size();)
		{
			wchar_t chw = text_[curCharInd_];
			switch (chw)
			{
			case L'.': // dot is the end of a sentence or the indicator of an abbreviation; the caller must decide
			case L'?':
			case L'!':
			case L'…': // horizontal ellipsis code=8230
				{
					finishTextRunIfStarted();
					createSingleCharRun(TextRunType::PunctuationStopSentence);
					return true;
				}
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
			case L'¬': // not sign (optional hyphen)
			case L'-': // dash
			case L'\'': // apostrophe dec=39
			case L'’': // right single quotation mark dec=8217
			case L'`': // grave accent (on the tilde key) dec=96
				finishTextRunIfStarted();
				createSingleCharRun(TextRunType::Punctuation);
				break;
				//case L'¬': // not sign (optional hyphen)
				//	// the word should be merged, as if this mark doesn't exist
				//	if (isWordStarted()) // if the word already started
				//	{
				//		outCharInd_ = charInd_;
				//	}
				//	break;
				//case L'-':
				//	//// if the dash is inside the word, then we have a compounded word
				//	//// otherwise it is a word separator
				//	//if (isWordStarted())
				//	//{
				//	//	onWordCharacter(chw);
				//	//	gotHyphen_ = true;
				//	//}
				//	//else
				//		onWordBreak(chw, tokens);
				//	break;

				//case L'\'': // apostrophe dec=39
				//case L'’': // right single quotation mark dec=8217
				//case L'`': // grave accent (on the tilde key) dec=96
				//	//// apostrophe at the word boundary is treated as a word separator
				//	//if (isWordStarted())
				//	//{
				//	//	// character of a word
				//	//	onWordCharacter(chw);
				//	//	gotApostrophe_ = true;
				//	//}
				//	//else
				//		onWordBreak(chw, tokens);
				//	break;
			case L'\t':
			case L'\r':
			case L'\n':
			case L' ': // space
			case L' ': // dec=160 no-break space (weird case after long hyphen, not a space)
				continueRun(TextRunType::Whitespace);
				curCharInd_ += 1;
				break;

			case L'0':
			case L'1':
			case L'2':
			case L'3':
			case L'4':
			case L'5':
			case L'6':
			case L'7':
			case L'8':
			case L'9':
				continueRun(TextRunType::Digit);
				curCharInd_ += 1;
				break;
			default:
				// alphabetical character
				continueRun(TextRunType::Alpha);
				curCharInd_ += 1;
				break;
			} // switch char
		}

		// finish the last word
		finishTextRunIfStarted();

		return false; // no text left to process
	}

	void TextParserNew::startTextRun(TextRunType curTextRun)
	{
		PG_DbgAssert2(textRunType_ == boost::none, "Prev text run must be finished");
		textRunStartInd_ = curCharInd_;
		textRunType_ = curTextRun;
	}

	bool TextParserNew::isWordStarted() const
	{
		return textRunStartInd_ != -1;
	}

	void TextParserNew::finishTextRunIfStarted()
	{
		if (!isWordStarted())
			return;
		finishTextRun();
	}

	void TextParserNew::finishTextRun()
	{
		//int wordEndIndExcl;
		//if (outCharInd_ != -1) // word shifting state
		//	wordEndIndExcl = outCharInd_;
		//else
		//	wordEndIndExcl = curCharInd_;

		// when word ends with apostrophe or hyphen, then treat apostrophe or hyphen as a separator
		//if (gotApostrophe_ || gotHyphen_)
		//	wordEndIndExcl -= 1;

		PG_DbgAssert2(textRunStartInd_ < curCharInd_, "Text run has at least one character");
		PG_DbgAssert(textRunDest_ != nullptr);

		int len = curCharInd_ - textRunStartInd_;
		RawTextRun token;
		token.Str = boost::wstring_view(&text_[textRunStartInd_], len);
		token.Type = textRunType_.value();
		textRunDest_->push_back(token);

		// prepare for new word
		textRunStartInd_ = -1;
		textRunType_ = boost::none;
		//outCharInd_ = -1;
		//gotApostrophe_ = false;
		//gotHyphen_ = false;
	}

	void TextParserNew::continueRun(TextRunType curTextRun)
	{
		if (textRunStartInd_ == -1)
		{
			startTextRun(curTextRun);
		}
		else
		{
			// there is an active text run
			if (textRunType_ == curTextRun)
			{
				// continue this text run
			}
			else
			{
				finishTextRun();
				startTextRun(curTextRun);
			}
		}

		//wchar_t outChar = chw;
		//if (chw == L'’' || chw == L'`')
		//{
		//	// normalize various ways to represent apostrophe
		//	outChar = L'\'';
		//}
		//else
		//{
		//	// normalize, so that all words are in lower case
		//	QChar qChar(chw);
		//	if (!qChar.isLower())
		//	{
		//		outChar = qChar.toLower().unicode();
		//	}
		//}

		//// x the character is overwritten if it is uppercase and need to be converted in a lowercase
		//// x there was an optional hypen (¬) in the word and characters after the optional hyphen must be shifted one symbol to the left
		//
		//if (outChar != chw ||
		//	outCharInd_ != -1) // shifting chars
		//{
		//	int putPos;
		//	if (outCharInd_ != -1)
		//	{
		//		putPos = outCharInd_;
		//		outCharInd_++;
		//	}
		//	else
		//		putPos = curCharInd_;
		//	text_[putPos] = outChar;
		//}
	}

	void TextParserNew::createSingleCharRun(TextRunType curTextRun)
	{
		startTextRun(curTextRun);
		curCharInd_ += 1; // next unprocessed char
		finishTextRun();
	}

	//

	void removeWhitespaceLexemes(std::vector<RawTextRun>& textRuns)
	{
		auto newEnd = std::remove_if(std::begin(textRuns), std::end(textRuns),
		                             [](auto& x) { return x.Type == TextRunType::Whitespace; });
		textRuns.erase(newEnd, std::end(textRuns));
	}

	void removeWhitespaceLexemes(std::vector<RawTextLexeme>& lexemes)
	{
		auto newEnd = std::remove_if(std::begin(lexemes), std::end(lexemes),
		                             [](auto& x) { return x.RunType == TextRunType::Whitespace; });
		lexemes.erase(newEnd, std::end(lexemes));
	}

	std::wstring toString(wv::slice<wchar_t> wordSlice)
	{
		return std::wstring(wordSlice.data(), wordSlice.size());
	}

	wchar_t pgToLower(wchar_t x)
	{
		return QChar(x).toLower().unicode();
	}

	void pgToLower(boost::wstring_view x, std::wstring* result)
	{
		result->resize(x.size());
		for (size_t i = 0; i < x.size(); ++i)
			(*result)[i] = pgToLower(x[i]);
	}

	namespace
	{
		struct CharMap
		{
			wchar_t chTo1;
			wchar_t chTo2;
			// chFrom is optimized away
			constexpr explicit CharMap(int chFromInt, wchar_t chFrom, wchar_t charTo)
				: chTo1(charTo), chTo2(0)
			{
			}

			constexpr explicit CharMap(int chFromInt, wchar_t chFrom, wchar_t charTo1, wchar_t charTo2)
				: chTo1(charTo1), chTo2(charTo2)
			{
			}
		};
	}

	void transliterateUaToEn(boost::wstring_view strUa, std::wstring* result)
	{
		result->reserve(strUa.size());
		for (size_t i = 0; i < strUa.size(); ++i)
		{
			const wchar_t ch = strUa[i];
			wchar_t mapCh1 = ch;
			wchar_t mapCh2 = 0;
			int chInt = (int)ch;
			if (chInt < 128)
			{
				// leave char as is
				mapCh1 = ch;
			}
			else if (chInt == 1028) // L'Є'
				mapCh1 = L'E';
			else if (chInt == 1030) // L'І'
				mapCh1 = L'I';
			else if (chInt == 1031) // L'Ї'
			{
				mapCh1 = L'J';
				mapCh2 = L'I';
			}
			else if (chInt >= 1040 && chInt <= 1103)
			{
				// 1040 L'А' till 1071 L'Я' and 1072 L'а' till 1103 'я'
				constexpr std::array<CharMap, 64> rang = {
					CharMap(1040, L'А', L'A'),
					CharMap(1041, L'Б', L'B'),
					CharMap(1042, L'В', L'V'),
					CharMap(1043, L'Г', L'H'),
					CharMap(1044, L'Д', L'D'),
					CharMap(1045, L'Е', L'E'),
					CharMap(1046, L'Ж', L'Z', L'H'),
					CharMap(1047, L'З', L'Z'),
					CharMap(1048, L'И', L'Y'), // collide UA and RU
					CharMap(1049, L'Й', L'J'),
					CharMap(1050, L'К', L'K'),
					CharMap(1051, L'Л', L'L'),
					CharMap(1052, L'М', L'M'),
					CharMap(1053, L'Н', L'N'),
					CharMap(1054, L'О', L'O'),
					CharMap(1055, L'П', L'P'),
					CharMap(1056, L'Р', L'R'),
					CharMap(1057, L'С', L'S'),
					CharMap(1058, L'Т', L'T'),
					CharMap(1059, L'У', L'U'),
					CharMap(1060, L'Ф', L'F'),
					CharMap(1061, L'Х', L'H'),
					CharMap(1062, L'Ц', L'C'),
					CharMap(1063, L'Ч', L'C', L'H'),
					CharMap(1064, L'Ш', L'S', L'H'),
					CharMap(1065, L'Щ', L'S', L'H'),
					CharMap(1066, L'Ъ', 0),
					CharMap(1067, L'Ы', L'Y'),
					CharMap(1068, L'Ь', 0),
					CharMap(1069, L'Э', L'E'),
					CharMap(1070, L'Ю', L'J', L'U'),
					CharMap(1071, L'Я', L'J', L'A'),
					CharMap(1072, L'а', L'a'),
					CharMap(1073, L'б', L'b'),
					CharMap(1074, L'в', L'v'),
					CharMap(1075, L'г', L'h'),
					CharMap(1076, L'д', L'd'),
					CharMap(1077, L'е', L'e'),
					CharMap(1078, L'ж', L'z', L'h'),
					CharMap(1079, L'з', L'z'),
					CharMap(1080, L'и', L'y'), // collide UA and RU
					CharMap(1081, L'й', L'j'),
					CharMap(1082, L'к', L'k'),
					CharMap(1083, L'л', L'l'),
					CharMap(1084, L'м', L'm'),
					CharMap(1085, L'н', L'n'),
					CharMap(1086, L'о', L'o'),
					CharMap(1087, L'п', L'p'),
					CharMap(1088, L'р', L'r'),
					CharMap(1089, L'с', L's'),
					CharMap(1090, L'т', L't'),
					CharMap(1091, L'у', L'u'),
					CharMap(1092, L'ф', L'f'),
					CharMap(1093, L'х', L'h'),
					CharMap(1094, L'ц', L'c'),
					CharMap(1095, L'ч', L'c', L'h'),
					CharMap(1096, L'ш', L's', L'h'),
					CharMap(1097, L'щ', L's', L'h'),
					CharMap(1098, L'ъ', 0),
					CharMap(1099, L'ы', L'y'),
					CharMap(1100, L'ь', 0),
					CharMap(1101, L'э', L'e'),
					CharMap(1102, L'ю', L'j', L'u'),
					CharMap(1103, L'я', L'j', L'a')
				};
				//static_assert(rang[0].chTo1 == L'A');
				int off = chInt - 1040;
				const CharMap& to = rang[off];
				mapCh1 = to.chTo1;
				mapCh2 = to.chTo2;
			}
			else if (chInt == 1108) // L'є'
				mapCh1 = L'e';
			else if (chInt == 1110) // L'і'
				mapCh1 = L'i';
			else if (chInt == 1111) // L'ї'
			{
				mapCh1 = L'j';
				mapCh2 = L'i';
			}
			else if (chInt == 1168) // L'Ґ'
				mapCh1 = L'G';
			else if (chInt == 1169) // L'ґ'
				mapCh1 = L'g';
			else
				mapCh1 = L'_'; // fallback

			if (mapCh1 != 0) result->push_back(mapCh1);
			if (mapCh2 != 0) result->push_back(mapCh2);
		}
	}

	bool isApostrophe(wchar_t ch)
	{
		switch (ch)
		{
		case L'\'': // apostrophe dec=39
		case L'’': // right single quotation mark dec=8217
		case L'`': // grave accent (on the tilde key) dec=96
			return true;
		default:
			return false;
		}
	}

	bool isDigitChar(wchar_t ch)
	{
		int chCode = (int)ch;
		bool result = chCode >= (int)L'0' && chCode <= (int)L'9';
		return result;
	}

	bool isRomanNumeral(wchar_t ch)
	{
		// http://mathworld.wolfram.com/RomanNumerals.html
		if (ch == L'i' || ch == L'I' || // 1
			ch == L'v' || ch == L'V' || // 5
			ch == L'x' || ch == L'X') // 10
			return true;
		return false;
	}

	bool isEnglishChar(wchar_t ch)
	{
		int chCode = (int)ch;
		bool isLarge = chCode >= L'A' && chCode <= L'Z';
		bool isSmall = chCode >= L'a' && chCode <= L'z';
		return isSmall || isLarge;
	}

	bool isEnglishVowel(wchar_t ch, bool includeY)
	{
		// aeiouy
		if (ch == L'a' || ch == L'A' ||
			ch == L'e' || ch == L'E' ||
			ch == L'i' || ch == L'I' ||
			ch == L'o' || ch == L'O' ||
			ch == L'u' || ch == L'U')
			return true;

		// y is a vowel in 'cry' or 'candy'
		// y is a consonant in 'yellow'
		if (includeY && (ch == L'y' || ch == L'Y'))
			return true;
		return false;
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

	int vowelsCountUk(boost::wstring_view word)
	{
		int numVowels = 0;
		for (size_t i = 0; i < word.size(); ++i)
		{
			wchar_t ch = word[i];
			if (isUkrainianVowel(ch))
				numVowels++;
		}
		return numVowels;
	}


	boost::optional<CharGroup> classifyUkrainianChar(wchar_t ch)
	{
		bool isVowel = isUkrainianVowel(ch);
		bool isCons = isUkrainianConsonant(ch);
		if (isVowel && !isCons)
			return CharGroup::Vowel;
		else if (!isVowel && isCons)
			return CharGroup::Consonant;
		return boost::none;
	}

	//

	void AbbreviationExpanderUkr::expandInplace(std::vector<RawTextLexeme>& lexemes)
	{
		lexemes_ = &lexemes;
		for (curLexInd_ = 0; curLexInd_ < (int)lexemes_->size();)
		{
			bool anyRuleWasApplied = false;
			auto tryRule = [this, &anyRuleWasApplied](decltype(&AbbreviationExpanderUkr::ruleTemplate) ruleFun) -> void
				{
					if (anyRuleWasApplied)
						return;
					anyRuleWasApplied = (this ->* ruleFun)();
				};

			tryRule(&AbbreviationExpanderUkr::ruleRepairWordSplitByOptionalHyphen);
			tryRule(&AbbreviationExpanderUkr::ruleApostropheInsideWord);
			tryRule(&AbbreviationExpanderUkr::ruleExpandRomanNumber);
			tryRule(&AbbreviationExpanderUkr::ruleNumberTi);
			tryRule(&AbbreviationExpanderUkr::ruleNumberRoky);

			if (!anyRuleWasApplied)
				curLexInd_ += 1;
		}
		lexemes_ = nullptr;
		curLexInd_ = -1;
	}

	RawTextLexeme& AbbreviationExpanderUkr::curLex()
	{
		return nextLex(0);
	}

	RawTextLexeme& AbbreviationExpanderUkr::nextLex(int stepsCount)
	{
		RawTextLexeme* lex = hasNextLex(stepsCount) ? &(*lexemes_)[curLexInd_ + stepsCount] : nullptr;
		PG_Assert(lex != nullptr);
		return *lex;
	}

	bool AbbreviationExpanderUkr::hasNextLex(int stepsCount) const
	{
		auto nextInd = curLexInd_ + stepsCount;
		return nextInd >= 0 && nextInd < (int)lexemes_->size();
	}

	bool AbbreviationExpanderUkr::isWord(const RawTextLexeme& x)
	{
		// by now, the only class assigned is number
		// treat everything else as a word
		return x.RunType == TextRunType::Alpha;
	}

	/// Returns true if one string in memory is exactly after another string.
	bool areContinous(boost::wstring_view lhs, boost::wstring_view rhs)
	{
		return lhs.end() == rhs.begin();
	}

	int romanToArabicNumeral(boost::wstring_view romanNum)
	{
		auto romans = {
			L"I", L"II", L"III", L"IV", L"V", L"VI", L"VII", L"VIII", L"IX", L"X",
			L"XI", L"XII", L"XIII", L"XIV", L"XV", L"XVI", L"XVII", L"XVIII", L"XIX", L"XX", L"XXI"};
		auto it = std::find_if(std::begin(romans), std::end(romans), [romanNum](auto n) { return romanNum == n; });
		if (it == std::end(romans))
			return -1;

		auto ind = std::distance(std::begin(romans), it);
		return ind + 1;
	}

	bool expandRomanNumeral(boost::wstring_view romanNum, WordCase wordCase, WordGender gender, boost::wstring_view& wordNum)
	{
		// TODO: ukr='Х' code=1061 instead of lat='X' code=88
		int num = romanToArabicNumeral(romanNum);
		if (num == -1)
			return false;

		struct NumCases
		{
			int Num;
			const wchar_t* NumNominative;
			const wchar_t* NumGenitive;
			const wchar_t* NumDative;
		};
		auto neuter = {
			NumCases{12, L"дванадцяте", L"дванадцятого", L"дванадцятому"},
			NumCases{16, L"шістнадцяте", L"шістнадцятого", L"шістнадцятому"},
			NumCases{15, L"п'ятнадцяте", L"п'ятнадцятого", L"п'ятнадцятому"},
			NumCases{20, L"двадцяте", L"двадцятого", L"двадцятому"},
		};
		auto it = std::find_if(std::begin(neuter), std::end(neuter), [num](auto& n)
		                       {
			                       return n.Num == num;
		                       });
		if (it == std::end(neuter))
			return false;

		PG_Assert2(gender == WordGender::Neuter, "Not implemented");

		const NumCases& cases = *it;
		switch (wordCase)
		{
		case WordCase::Nominative:
			wordNum = cases.NumNominative;
			return true;
		case WordCase::Genitive:
			wordNum = cases.NumGenitive;
			return true;
		case WordCase::Dative:
			wordNum = cases.NumDative;
			return true;
		default:
			PG_Assert2(false, "Not implemented");
			return false;
		}
	}

	IntegerToUaWordConverter::IntegerToUaWordConverter()
	{
		declinedWords_ = std::make_unique<std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>>();
	}

	bool IntegerToUaWordConverter::load(const boost::filesystem::path& declensionDictPath, std::wstring* errMsg)
	{
		// base numbers
		std::array<BaseNumToWordMap, 37> baseNumbersArray = {
			BaseNumToWordMap(0, L"нуль", L"нульовий"),
			BaseNumToWordMap(1, L"один", L"перший"),
			BaseNumToWordMap(2, L"два", L"другий"),
			BaseNumToWordMap(3, L"три", L"третій"),
			BaseNumToWordMap(4, L"чотири", L"четвертий"),
			BaseNumToWordMap(5, L"п'ять", L"п'ятий"),
			BaseNumToWordMap(6, L"шість", L"шостий"),
			BaseNumToWordMap(7, L"сім", L"сьомий"),
			BaseNumToWordMap(8, L"вісім", L"восьмий"),
			BaseNumToWordMap(9, L"дев'ять", L"дев'ятий"),
			BaseNumToWordMap(10, L"десять", L"десятий"),
			BaseNumToWordMap(11, L"одинадцять", L"одинадцятий"),
			BaseNumToWordMap(12, L"дванадцять", L"дванадцятий"),
			BaseNumToWordMap(13, L"тринадцять", L"тринадцятий"),
			BaseNumToWordMap(14, L"чотирнадцять", L"чотирнадцятий"),
			BaseNumToWordMap(15, L"п'ятнадцять", L"п'ятнадцятий"),
			BaseNumToWordMap(16, L"шістнадцять", L"шістнадцятий"),
			BaseNumToWordMap(17, L"сімнадцять", L"сімнадцятий"),
			BaseNumToWordMap(18, L"вісімнадцять", L"вісімнадцятий"),
			BaseNumToWordMap(19, L"дев'ятнадцять", L"дев'ятнадцятий"),
			BaseNumToWordMap(20, L"двадцять", L"двадцятий"),
			BaseNumToWordMap(30, L"тридцять", L"тридцятий"),
			BaseNumToWordMap(40, L"сорок", L"сороковий"),
			BaseNumToWordMap(50, L"п'ятдесят", L"п'ятдесятий"),
			BaseNumToWordMap(60, L"шістдесят", L"шістдесятий"),
			BaseNumToWordMap(70, L"сімдесят", L"сімдесятий"),
			BaseNumToWordMap(80, L"вісімдесят", L"вісімдесятий"),
			BaseNumToWordMap(90, L"дев'яносто", L"дев'яностий"),
			BaseNumToWordMap(100, L"сто", L"сотий"),
			BaseNumToWordMap(200, L"двісті", L"двохсотий"),
			BaseNumToWordMap(300, L"триста", L"трьохсотий"),
			BaseNumToWordMap(400, L"чотириста", L"чотирьохсотий"),
			BaseNumToWordMap(500, L"п'ятсот", L"п'ятисотий"),
			BaseNumToWordMap(600, L"шістсот", L"шестисотий"),
			BaseNumToWordMap(700, L"сімсот", L"семисотий"),
			BaseNumToWordMap(800, L"вісімсот", L"восьмисотий"),
			BaseNumToWordMap(900, L"дев'ятсот", L"дев'ятисотий")
		};
		std::copy(std::begin(baseNumbersArray), std::end(baseNumbersArray), std::back_inserter(baseNumbers_));

		// zero words, num=log10(zeroWord)
		std::array<BaseNumToWordMap, 6> zeroWordsArray = {
			BaseNumToWordMap(0, L"", L""), // placeholder to keep thousand in index=1
			BaseNumToWordMap(3, WordGender::Feminine, L"тисяча", L"тисячний"),
			BaseNumToWordMap(6, WordGender::Masculine, L"мільйон", L"мільйонний"),
			BaseNumToWordMap(9, WordGender::Masculine, L"мільярд", L"мільярдний"),
			BaseNumToWordMap(12, WordGender::Masculine, L"трильйон", L"трильйонний"),
			BaseNumToWordMap(15, WordGender::Masculine, L"квадрильйон", L"квадрильйонний"),
		};
		std::copy(std::begin(zeroWordsArray), std::end(zeroWordsArray), std::back_inserter(zerosWords_));

		//
		return loadUkrainianWordDeclensionXml(declensionDictPath.wstring(), *declinedWords_, errMsg);
	}

	//
	void IntegerToUaWordConverter::convertBaseNumber(gsl::span<const BaseNumToWordMap> numInfos, int baseNum, NumeralCardinality cardOrd, WordCase wordCase, 
		boost::optional<EntityMultiplicity> mult, boost::optional<WordGender> gender, bool compoundNumeral, std::vector<RawTextLexeme>& lexemes)
	{
		auto numInfoIt = std::find_if(std::begin(numInfos), std::end(numInfos), [baseNum](auto& x) { return x.Num == baseNum; });
		PG_Assert(numInfoIt != std::end(numInfos));

		const BaseNumToWordMap& numInfo = *numInfoIt;
		boost::wstring_view numName = cardOrd == NumeralCardinality::Cardinal ? numInfo.NumCardinal : numInfo.NumOrdinal;

		auto numIt = declinedWords_->find(std::wstring(numName.data()));
		PG_Assert(numIt != std::end(*declinedWords_));

		auto matchForm = [=](const WordDeclensionForm& form) -> bool
		{
			if (form.Case != boost::none && form.Case != wordCase)
				return false;
			if (form.Gender != boost::none && form.Gender != gender)
				return false;
			if (form.NumberCategory != boost::none && form.NumberCategory != mult)
				return false;
			return true;
		};

		const std::unique_ptr<WordDeclensionGroup>& group = numIt->second;
		std::vector<const WordDeclensionForm*> forms;
		for (const WordDeclensionForm& form : group->Forms)
		{
			if (matchForm(form))
				forms.push_back(&form);
		}

		bool unique = forms.size() == 1;
		auto errMsg = [=]() { return QString("No form found for num=%1(U=%2) card=%3 case=%4 mult=%5 gender=%6")
			.arg(baseNum)
			.arg(numInfos.size())
			.arg(toQString(toString(cardOrd)))
			.arg(toQString(toString(wordCase)))
			.arg(mult != boost::none ? toQString(toString(mult.value())) : QString::fromLatin1("none"))
			.arg(gender != boost::none ? toQString(toString(gender.value())) : QString::fromLatin1("none")); };
		PG_Assert2(unique, errMsg().toStdWString().c_str());
		const WordDeclensionForm& form = *forms.front();

		// trim alternative cases "п'ятдесяти,п'ятдесятьох"
		boost::wstring_view numStr = form.Name;
		auto commaPos = numStr.find(L',');
		if (commaPos != boost::wstring_view::npos)
			numStr = boost::wstring_view(form.Name.data(), commaPos);

		// rule: одного->одно for composite numerals
		// числівник один у складних словах набуває форми одно-: одноліток, однолюб, одноколірний; 
		// http://mova.kreschatic.kiev.ua/24.php
		if (baseNum == 1 && wordCase == WordCase::Genitive && gender == WordGender::Masculine &&
			compoundNumeral)
		{
			PG_DbgAssert(numStr == L"одного");
			// одноготисячний->однотисячний
			numStr = L"одно";
		}

		RawTextLexeme lex;
		lex.ValueStr = numStr;
		lex.RunType = TextRunType::Alpha;
		lex.Lang = TextLanguage::Ukrainian;
		lex.Class = WordClass::Numeral;
		lex.Case = form.Case;
		lex.Gender = form.Gender;
		lex.NumeralLexView = NumeralLexicalView::Word;
		lexemes.push_back(lex);
	}

	/// Find the last non-zero digit of a number and returns it's 1-based index from the right side.
	/// Result=0 coresponds to the last digit, 1 corresponds to the second from the last.
	/// For 6900 returns 2.
	int lastNonZeroDecimalInd(ptrdiff_t num)
	{
		ptrdiff_t magn = 10;
		for (int order = 0; ; order += 1)
		{
			int digit = num % magn;
			if (digit > 0)
				return order;
			magn *= 10;
		}
	}

	void IntegerToUaWordConverter::convertOneTriple(ptrdiff_t triple, NumeralCardinality tripleCardOrd, WordCase tripleCase, boost::optional<EntityMultiplicity> tripleMult, boost::optional<WordGender> gender,
		bool compoundOrdinal, bool isLastTriple, std::vector<RawTextLexeme>& lexemes)
	{
		PG_DbgAssert(triple < 1000);

		int nonzOrd = lastNonZeroDecimalInd(triple);

		int magnit = static_cast<int>(std::trunc(std::log10f(triple)));
		int mul10 = static_cast<int>(std::pow(10, magnit)); // ..., 1000, 100, 10, 1

		int curDecimInd = magnit;

		int numTail = triple;
		for (; numTail > 0;)
		{
			int baseNum;
			bool isLastBaseNum;
			if (numTail < 20) // base numbers below 20 are well known
			{
				baseNum = numTail;
				isLastBaseNum = true;
			}
			else
			{
				baseNum = static_cast<int>(std::trunc(numTail / mul10)) * mul10;
				isLastBaseNum = curDecimInd == nonzOrd;
			}

			// skip zeros inside the triple
			if (baseNum != 0)
			{
				WordCase decimCase = tripleCase;
				NumeralCardinality decimCardOrd = tripleCardOrd;
				// only the last word is declined for ordinal numerals
				if (tripleCardOrd == NumeralCardinality::Ordinal)
				{
					if (compoundOrdinal)
					{
						// all decimals of the triple are in genitive case
						// 825000=восьмисотдвадцятип'ятитисячний
						// 825300=вісімсот двадцять п'ять тисяч трьохсотий
						decimCase = WordCase::Genitive;
						decimCardOrd = NumeralCardinality::Cardinal;
					}
					else
					{
						// the higher two decimals are in nominative case
						// the last decimal is in a requested case
						if (isLastBaseNum)
						{
							decimCase = tripleCase;
							decimCardOrd = tripleCardOrd;
						}
						else
						{
							decimCase = WordCase::Nominative;
							decimCardOrd = NumeralCardinality::Cardinal;
						}
					}
				}

				convertBaseNumber(baseNumbers_, baseNum, decimCardOrd, decimCase, tripleMult, gender, compoundOrdinal, lexemes);

				// every decimal is separated by space, except the single word ordinal numbers ending in 1k, 1mln, 1bln
				bool avoidSpace = isLastBaseNum || // put the space only in the middle of the tripple
					(compoundOrdinal && isLastTriple);
				if (!avoidSpace)
				{
					RawTextLexeme lex;
					lex.ValueStr = L" ";
					lex.RunType = TextRunType::Whitespace;
					lexemes.push_back(lex);
				}
			}

			numTail -= baseNum;
			mul10 /= 10;
			curDecimInd -= 1;
		}
		PG_DbgAssert(numTail == 0);
	}

	/// Multplicity is applicable only for ordinal numbers (which refer to one element), to zero words
	/// (which are nouns) to numeral 'one' (which is also a single entity).
	bool IntegerToUaWordConverter::convert(ptrdiff_t num, NumeralCardinality cardOrd, WordCase numCase, boost::optional<EntityMultiplicity> multiplicity, boost::optional<WordGender> gender, std::vector<RawTextLexeme>& lexemes, std::wstring* errMsg)
	{
		if (num < 0)
			return false; // not implemented
		if (multiplicity != boost::none)
		{
			bool ok = cardOrd == NumeralCardinality::Ordinal ||
				num == 0 || num == 1 || num % 1000 == 0;
			if (!ok)
			{
				//*errMsg = L"Multiplicity is only applicable to numbers refering to one entity (or nouns like 10^3k)";
				//return false;
			}
		}
		if (num == 0)
		{
			convertBaseNumber(baseNumbers_, num, cardOrd, numCase, multiplicity, gender, false, lexemes);
			return true;
		}

		int nonzOrd = lastNonZeroDecimalInd(num);
		int nonzTripleInd = nonzOrd / 3;

		// стотисячний, -мільйонний, -мільйардний
		bool compoundOrdinal = false;
		if (nonzTripleInd >= 1 && cardOrd == NumeralCardinality::Ordinal)
			compoundOrdinal = true;

		//
		int magnit = static_cast<int>(std::trunc(std::log10f(num)));
		int curTripleInd = magnit / 3;
		if (curTripleInd >= (int)zerosWords_.size())
		{
			*errMsg = L"The number is too large to represent as a text";
			return false;
		}

		int trimTriplesCount = curTripleInd;
		auto mul10Tripple = static_cast<ptrdiff_t>(std::pow(1000, trimTriplesCount)); // ..., 1000000, 1000, 1

		//

		auto numTail = num;
		for (; numTail > 0;)
		{
			bool isLastTriple = curTripleInd == nonzTripleInd;
			int triple = numTail / mul10Tripple;

			WordCase tripleCase = numCase;
			NumeralCardinality tripleCardOrd = cardOrd;
			// only the last word is declined for ordinal numerals
			if (cardOrd == NumeralCardinality::Ordinal && !isLastTriple)
			{
				// 825346=вісімсот двадцять п'ять тисяч триста сорок шостий
				tripleCase = WordCase::Nominative;
				tripleCardOrd = NumeralCardinality::Cardinal;
			}

			bool hasZerosWord = curTripleInd > 0;
			if (hasZerosWord)
			{
				const BaseNumToWordMap& zerosWord = zerosWords_[curTripleInd];

				// last base number of a triple
				int nonzInd = lastNonZeroDecimalInd(triple);
				auto mul10 = static_cast<ptrdiff_t>(std::pow(10, nonzInd + 1));
				ptrdiff_t lastBaseNum = triple % mul10;
				

				boost::optional<EntityMultiplicity> tripleMult;
				boost::optional<EntityMultiplicity> zerosMult;
				boost::optional<WordGender> trippleGender;
				if (compoundOrdinal && isLastTriple)
				{
					tripleMult = multiplicity;
					trippleGender = gender;
					zerosMult = multiplicity;
				}
				else
				{
					trippleGender = zerosWord.Gender;

					// multiplicity have sense only for cardinal numbers
					if (tripleCardOrd == NumeralCardinality::Ordinal)
					{
						//if (lastBaseNum == 1)
						//	tripleMult = EntityMultiplicity::Singular;
						//else if (lastBaseNum == 2)
						//	tripleMult = EntityMultiplicity::Plural;
					}
					zerosMult = lastBaseNum == 1 ? EntityMultiplicity::Singular : EntityMultiplicity::Plural;
				}

				//
				convertOneTriple(triple, tripleCardOrd, tripleCase, tripleMult, trippleGender, compoundOrdinal, isLastTriple,  lexemes);

				bool avoidSpace = compoundOrdinal && isLastTriple;
				if (!avoidSpace)
				{
					// space
					RawTextLexeme lex;
					lex.ValueStr = L" ";
					lex.RunType = TextRunType::Whitespace;
					lexemes.push_back(lex);
				}

				// numerals 5-19, 20,30,...,100 have specific nominative and acuastive cases
				// http://shkola.ostriv.in.ua/publication/code-1F90DA0EF2D02/list-B8AFBC4326
				WordCase tripleCaseFix = tripleCase;
				bool isDozen = lastBaseNum % 10 == 0 && lastBaseNum <= 100; // 10, 20, ... 100
				if (lastBaseNum >= 5 && lastBaseNum < 20 || isDozen)
				{
					if ((tripleCase == WordCase::Nominative || tripleCase == WordCase::Acusative) &&
						zerosMult == EntityMultiplicity::Plural)
						tripleCaseFix = WordCase::Genitive;
				}

				convertBaseNumber(zerosWords_, zerosWord.Num, tripleCardOrd, tripleCaseFix, zerosMult, trippleGender, compoundOrdinal, lexemes);
			}
			else
			{
				convertOneTriple(triple, tripleCardOrd, tripleCase, multiplicity, gender, compoundOrdinal, isLastTriple, lexemes);
			}

			bool avoidSpace = isLastTriple;
			if (!avoidSpace)
			{
				// space
				RawTextLexeme lex;
				lex.ValueStr = L" ";
				lex.RunType = TextRunType::Whitespace;
				lexemes.push_back(lex);
			}

			//
			numTail %= mul10Tripple;
			curTripleInd -= 1;
			mul10Tripple /= 1000;
		}
		PG_DbgAssert(numTail == 0);

		return true;
	}

	bool AbbreviationExpanderUkr::ruleApostropheInsideWord()
	{
		if (!hasNextLex(-1) || !hasNextLex(1)) return false;
		auto& lhs = nextLex(-1);
		auto& rhs = nextLex(1);
		if (isApostrophe(nextLex(0).ValueStr.front()) &&
			isWord(lhs) &&
			isWord(rhs))
		{
			std::wstring buf;
			buf.append(lhs.ValueStr.data(), lhs.ValueStr.size());
			buf.push_back(LetterUk::apostrophe());
			buf.append(rhs.ValueStr.data(), rhs.ValueStr.size());

			boost::wstring_view arenaWord;
			registerWordThrow<wchar_t>(*stringArena_, buf, &arenaWord);

			// put result in the left word's part
			lhs.ValueStr = arenaWord;
			lexemes_->erase(lexemes_->begin() + curLexInd_ + 1); // right word's part
			lexemes_->erase(lexemes_->begin() + curLexInd_); // apostrophe itself
			// cur pos is not changed
			return true;
		}
		return false;
	}

	bool AbbreviationExpanderUkr::ruleExpandRomanNumber()
	{
		// "в ^XX ст." where ^ marks cur lex
		if (nextLex(0).Class != WordClass::Numeral)
			return false;

		if (!hasNextLex(-2) || !hasNextLex(3)) return false;

		boost::optional<WordCase> numCaseLeft = boost::none;
		std::wstring prepos;
		pgToLower(nextLex(-2).ValueStr, &prepos);
		if (prepos == L"на")
			numCaseLeft = WordCase::Nominative;
		else if (
			prepos == L"початку" ||
			prepos == L"початком" ||
			prepos == L"середині" ||
			prepos == L"половині" ||
			prepos == L"наприкінці" ||
			prepos == L"кінця" ||
			prepos == L"до" ||
			prepos == L"рубежі" ||
			prepos == L"років")
			numCaseLeft = WordCase::Genitive;
		else if (prepos == L"в" || prepos == L"у")
			numCaseLeft = WordCase::Dative;
		if (numCaseLeft == boost::none)
			return false;

		bool rightContext = false;
		bool collapseDot = false;
		if (nextLex(2).StrLowerCase() == L"ст" &&
			nextLex(3).ValueStr[0] == L'.')
		{
			rightContext = true;
			collapseDot = true;
		}

		if (nextLex(-1).RunType == TextRunType::Whitespace &&
			nextLex(1).RunType == TextRunType::Whitespace && rightContext)
		{
			boost::wstring_view wordNum;
			if (!expandRomanNumeral(nextLex(0).ValueStr, numCaseLeft.value(), WordGender::Neuter, wordNum))
			{
				PG_Assert2(false, QString("Unknown roman %1").arg(toQString(wordNum)).toStdWString().c_str());
				return false;
			}

			RawTextLexeme numLex = nextLex(0);
			numLex.ValueStr = wordNum;
			numLex.Case = numCaseLeft;

			// TODO: сторіччі або столітті?
			std::array<const wchar_t*, 3> nounStrs = {L"століття", L"століття", L"столітті"};
			const wchar_t* nounStr = nullptr;
			if (numCaseLeft == WordCase::Nominative)
				nounStr = nounStrs[0];
			else if (numCaseLeft == WordCase::Genitive)
				nounStr = nounStrs[0];
			else if (numCaseLeft == WordCase::Dative)
				nounStr = nounStrs[2];
			PG_DbgAssert(nounStr != nullptr);

			RawTextLexeme nounLex = nextLex(2);
			nounLex.ValueStr = nounStr;
			nounLex.Class = WordClass::Noun;
			nounLex.Case = numCaseLeft;
			nounLex.Gender = WordGender::Neuter;

			// modify
			nextLex(-2).Class = WordClass::Preposition;
			nextLex(0) = numLex;
			nextLex(2) = nounLex;
			auto newEndIt = lexemes_->erase(lexemes_->begin() + curLexInd_ + 3);
			curLexInd_ += 3;
			return true;
		}
		return false;
	}

	bool AbbreviationExpanderUkr::ruleNumberTi()
	{
		// "У ^90-ті роки" where ^ marks cur lex
		if (nextLex(0).Class != WordClass::Numeral)
			return false;

		if (!hasNextLex(2)) return false;
		auto& lex = nextLex(0);
		int num = -1;
		if (!boost::conversion::try_lexical_convert(lex.ValueStr, num))
			return false;

		if (nextLex(1).RunType == TextRunType::Punctuation &&
			nextLex(2).ValueStr == L"ті")
		{
			const wchar_t* numStr = nullptr;
			switch (num)
			{
			case 10: numStr = L"десяті";
				break;
			case 20: numStr = L"тридцяті";
				break;
			case 30: numStr = L"двадцяті";
				break;
			case 50: numStr = L"п'ятдесяті";
				break;
			case 60: numStr = L"шістдесяті";
				break;
			case 70: numStr = L"сімдесяті";
				break;
			case 80: numStr = L"вісімдесяті";
				break;
			case 90: numStr = L"дев'яності";
				break;
			default: numStr = nullptr;
			}
			if (numStr == nullptr)
			{
				PG_Assert2(false, QString("Unknown roman %1").arg(toQString(lex.ValueStr)).toStdWString().c_str());;
				return false;
			}

			// modify
			nextLex(0).ValueStr = numStr;
			lexemes_->erase(lexemes_->begin() + curLexInd_ + 2); // ti
			lexemes_->erase(lexemes_->begin() + curLexInd_ + 1); // hyphen
			curLexInd_ += 1;
			return true;
		}

		return false;
	}

	bool AbbreviationExpanderUkr::ruleNumberRoky()
	{
		// "У дев'яності ^рр." where ^ marks cur lex
		if (!hasNextLex(-2) || !hasNextLex(1)) return false;
		if (nextLex(-2).Class == WordClass::Numeral &&
			nextLex(-1).RunType == TextRunType::Whitespace &&
			nextLex(0).ValueStr == L"рр" &&
			nextLex(1).ValueStr == L".")
		{
			// modify
			nextLex(0).ValueStr = L"роки";
			lexemes_->erase(lexemes_->begin() + curLexInd_ + 1); // dot
			curLexInd_ += 1;
			return true;
		}

		return false;
	}

	bool AbbreviationExpanderUkr::ruleRepairWordSplitByOptionalHyphen()
	{
		// "^ty¬coon"->"tycoon^" where ^ marks cur lex
		if (!hasNextLex(-1) || !hasNextLex(1)) return false;

		auto& lhs = nextLex(-1);
		auto& rhs = nextLex(1);
		if (nextLex(0).ValueStr.front() == L'¬' &&
			isWord(lhs) &&
			isWord(rhs))
		{
			PG_DbgAssert2(
				areContinous(lhs.ValueStr, nextLex(0).ValueStr) &&
				areContinous(nextLex(0).ValueStr, rhs.ValueStr), "Hyphen must split words without spaces from any side");

			// cut the optional hyphen
			std::wstring buf;
			buf.append(lhs.ValueStr.data(), lhs.ValueStr.size());
			buf.append(rhs.ValueStr.data(), rhs.ValueStr.size());

			boost::wstring_view arenaWord;
			registerWordThrow<wchar_t>(*stringArena_, buf, &arenaWord);

			// put result in the left word's part
			lhs.ValueStr = arenaWord;
			lexemes_->erase(lexemes_->begin() + curLexInd_ + 1); // right word's part
			lexemes_->erase(lexemes_->begin() + curLexInd_); // hyphen itself
			// cur pos is not changed
			return true;
		}
		return false;
	}

	void analyzeSentenceHelper(gsl::span<const RawTextRun> words, std::vector<RawTextLexeme>& lexemes)
	{
		for (int i = 0; i < words.size(); ++i)
		{
			const RawTextRun& textRun = words[i];
			boost::wstring_view wordSlice = textRun.Str;
			PG_Assert(!wordSlice.empty());

			int digitsCount = 0;
			int romanChCount = 0;
			int engCount = 0;
			int exclEngCount = 0;
			int rusCount = 0;
			int exclRusCount = 0;
			int hyphenCount = 0;

			auto wordCharUsage = [&](wv::slice<wchar_t> word)
				{
					for (size_t charInd = 0; charInd < word.size(); ++charInd)
					{
						wchar_t ch = word[charInd];
						bool isDigit = isDigitChar(ch);
						if (isDigit)
							digitsCount += 1;
						bool isRomanCh = isRomanNumeral(ch);
						if (isRomanCh)
							romanChCount += 1;
						bool isEng = isEnglishChar(ch);
						if (isEng)
							engCount++;
						bool isExclEng = isExclusiveEnglishChar(ch);
						if (isExclEng)
							exclEngCount++;
						bool isRus = isRussianChar(ch);
						if (isRus)
							rusCount++;
						bool isExclRus = isExclusiveRussianChar(ch);
						if (isExclRus)
							exclRusCount++;
						if (ch == L'-' || ch == L'\'')
							hyphenCount++;
					}
				};
			wordCharUsage(wordSlice);

			RawTextLexeme lex;
			lex.RunType = textRun.Type;
			lex.ValueStr = boost::wstring_view(wordSlice.data(), wordSlice.size());

			// output non-leterate words with digits, cryptic symbols, etc.
			if (digitsCount == wordSlice.size()) // Arabic number
			{
				lex.Class = WordClass::Numeral;
				lex.NumeralLexView = NumeralLexicalView::Arabic;
			}
			else if (romanChCount == wordSlice.size()) // Roman number
			{
				lex.Class = WordClass::Numeral;
				lex.NumeralLexView = NumeralLexicalView::Roman;
			}
			else if (exclEngCount > 0 && (engCount + hyphenCount) == wordSlice.size()) // english word
			{
				lex.Lang = TextLanguage::English;
			}
			else if (exclRusCount > 0 && (rusCount + hyphenCount) == wordSlice.size()) // russian word
			{
				lex.Lang = TextLanguage::Russian;
			}
			else if (digitsCount > 0 || romanChCount > 0 || exclEngCount > 0 || exclRusCount > 0)
				lex.NonLiterate = true;

			lexemes.push_back(lex);
		}
	}

	void registerInArena(GrowOnlyPinArena<wchar_t>& stringArena, gsl::span<const RawTextLexeme> inWords, std::vector<RawTextLexeme>& outWords)
	{
		for (const RawTextLexeme& lex : inWords)
		{
			RawTextLexeme newLex = lex;
			auto newStr = lex.ValueStr;
			registerWordThrow(stringArena, lex.ValueStr, &newLex.ValueStr);
			outWords.push_back(newLex);
		}
	}

	SentenceParser::SentenceParser(size_t stringArenaLineSize)
		: stringArena_(std::make_unique<GrowOnlyPinArena<wchar_t>>(stringArenaLineSize))
	{
	}

	void SentenceParser::setTextBlockReader(TextBlockReader* textReader)
	{
		textReader_ = textReader;
	}

	void SentenceParser::setOnNextSentence(OnNextSentence onNextSent)
	{
		onNextSent_ = onNextSent;
	}

	void SentenceParser::run()
	{
		// static const bool allowPartialSent = true;
		std::vector<wchar_t> textBuff;
		std::vector<RawTextLexeme> sentTmp;
		std::vector<RawTextRun> tillStopWords;
		std::vector<RawTextLexeme> tillStopLexemes;

		auto notifySent = [&]()
			{
				gsl::span<const RawTextLexeme> sent = sentTmp;
				if (onNextSent_ != nullptr) onNextSent_(sent);
				sentTmp.clear();
				stringArena_->clear();
			};

		while (true) // read entire textbuffer
		{
			textBuff.clear();
			if (!textReader_->nextBlock(textBuff))
				break;

			TextParserNew wordsReader;
			wordsReader.setInputText(boost::wstring_view(textBuff.data(), textBuff.size()));
			while (true) // read entire text block
			{
				tillStopWords.clear();
				bool tillDot = wordsReader.parseTokensTillDot(tillStopWords);
				if (!tillDot && tillStopWords.empty())
					break;

				tillStopLexemes.clear();
				analyzeSentenceHelper(tillStopWords, tillStopLexemes);

				AbbreviationExpanderUkr abbrExp;
				abbrExp.stringArena_ = stringArena_.get();
				abbrExp.expandInplace(tillStopLexemes);

				registerInArena(*stringArena_, tillStopLexemes, sentTmp);

				if (!sentTmp.empty() &&
					(sentTmp.back().ValueStr == L"." || sentTmp.back().ValueStr == L"!" || sentTmp.back().ValueStr == L"?"))
				{
					notifySent();
				}
			}
		}

		// flush what is left as a sentence
		if (!sentTmp.empty())
		{
			notifySent();
		}
	}
}
