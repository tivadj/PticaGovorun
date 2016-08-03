#pragma once
#include "PticaGovorunCore.h"
#include "PhoneticService.h"

namespace PticaGovorun
{
	/// For a word with multiple pronunciations checks if none or all pronCodes have marked stress.
	/// eg. valid=(word=setup, setup(1) setup(2)) or wrong=(word=setup, setup setup(1))
	PG_EXPORTS bool rulePhoneticDictNoneOrAllPronCodesMustHaveStress(const std::vector<PhoneticWord>& phoneticDictPronCodes, std::vector<const PhoneticWord*>* invalidWords);
}