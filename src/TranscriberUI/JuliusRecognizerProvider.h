#pragma once
#include <memory>

#ifdef PG_HAS_JULIUS
#include "JuliusToolNativeWrapper.h"

namespace PticaGovorun
{
	PG_EXPORTS bool initRecognizerConfiguration(boost::string_view recogName, RecognizerSettings& rs);

	class RecognizerNameHintProvider
	{
	public:
		virtual ~RecognizerNameHintProvider() { }

		// Gets what is the user's prefered recognizer name.
		virtual boost::string_view recognizerNameHint() = 0;
	};

	class JuliusRecognizerProvider
	{
	public:
		bool hasError() const;
		JuliusToolWrapper* instance(boost::string_view recogNameHint);

	private:
		void ensureRecognizerIsCreated(boost::string_view recogNameHint);
	private:
		std::unique_ptr<JuliusToolWrapper> recognizer_;
		std::string errorString_;
	};
}
#endif