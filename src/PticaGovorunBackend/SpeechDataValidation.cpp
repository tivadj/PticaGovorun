#include "stdafx.h"
#include "SpeechDataValidation.h"

namespace PticaGovorun
{
	bool rulePhoneticDictNoneOrAllPronCodesMustHaveStress(const std::vector<PhoneticWord>& phoneticDictPronCodes, std::vector<const PhoneticWord*>* invalidWords)
	{
		bool result = true;
		for (const PhoneticWord& word : phoneticDictPronCodes)
		{
			if (word.Pronunciations.size() < 2)
				continue;

			int numPronWithExclam = 0;
			for (const PronunciationFlavour& pron : word.Pronunciations)
			{
				if (pron.PronCode.ends_with(L')'))
					numPronWithExclam++;
			}
			bool ok = numPronWithExclam == 0 || numPronWithExclam == word.Pronunciations.size();
			if (!ok)
			{
				result = false;
				if (invalidWords != nullptr)
					invalidWords->push_back(&word);
			}
		}
		return result;
	}
}