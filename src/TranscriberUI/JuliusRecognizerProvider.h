#pragma once
#include <memory>

#ifdef PG_HAS_JULIUS
#include "JuliusToolNativeWrapper.h"

namespace PticaGovorun
{
	class RecognizerNameHintProvider
	{
	public:
		virtual ~RecognizerNameHintProvider() { }

		// Gets what is the user's prefered recognizer name.
		virtual std::string recognizerNameHint() = 0;
	};

	class JuliusRecognizerProvider
	{
	public:
		bool hasError() const;
		JuliusToolWrapper* instance(const std::string& recogNameHint);

	private:
		void ensureRecognizerIsCreated(const std::string& recogNameHint);
	private:
		std::unique_ptr<JuliusToolWrapper> recognizer_;
		std::string errorString_;
	};
}
#endif