#pragma once
#include <vector>
#include "PticaGovorunCore.h"
#include "ClnUtils.h"

namespace PticaGovorun
{
	// Creates copy of a string, specified by two pointers.
	PG_EXPORTS std::wstring toString(wv::slice<wchar_t> wordSlice);

	// Extrcts sentences from text. Skips commas and other punctuation marks.
	// Sentence is a sequence of words.
	class PG_EXPORTS TextParser
	{
	public:
		// Sets the buffer to read sentence (words) from.
		void setInputText(wv::slice<wchar_t> text);

		// Finds words' boundaries and return pointers to it.
		// The words are mutated to be lower
		// Returns false if there is no sentences left in the buffer.
		bool parseSentence(std::vector<wv::slice<wchar_t>>& words);

	private:
		void finishWord(std::vector<wv::slice<wchar_t>>& words);

		void onWordCharacter(wchar_t chw);

		void onWordBreak(wchar_t chw, std::vector<wv::slice<wchar_t>>& words);

		// Whether the character cursor points inside some word.
		bool isWordStarted() const;

	private:
		wv::slice<wchar_t> mutText_;
		int charInd_;
		int wordStartInd_;
		
		// -1 if chars are copied without position shift
		// When there is a character in the middle of a word which must be removed from output
		// this index tracks the output char position
		int outCharInd_; 

		// True, if current character is an apostrophe and we expect the next character to be something different.
		bool gotApostrophe_;
	};
}
