#include "JuliusRecognizerProvider.h"

#ifdef PG_HAS_JULIUS
namespace PticaGovorun
{
	bool JuliusRecognizerProvider::hasError() const
	{
		return !errorString_.empty();
	}

	JuliusToolWrapper* JuliusRecognizerProvider::instance(const std::string& recogNameHint)
	{
		if (hasError())
			return nullptr;
		ensureRecognizerIsCreated(recogNameHint);
		if (hasError())
			return nullptr;
		return recognizer_.get();
	}

	void JuliusRecognizerProvider::ensureRecognizerIsCreated(const std::string& recogNameHint)
	{
		// initialize the recognizer lazily
		if (recognizer_ == nullptr)
		{
			RecognizerSettings rs;
			if (!initRecognizerConfiguration(recogNameHint, rs))
			{
				errorString_ = "Can't find speech recognizer configuration";
				return;
			}

			QTextCodec* pTextCodec = QTextCodec::codecForName(PGEncodingStr);
			auto textCodec = std::unique_ptr<QTextCodec, NoDeleteFunctor<QTextCodec>>(pTextCodec);

			std::tuple<bool, std::string, std::unique_ptr<JuliusToolWrapper>> newRecogOp =
				createJuliusRecognizer(rs, std::move(textCodec));
			if (!std::get<0>(newRecogOp))
			{
				errorString_ = std::get<1>(newRecogOp);
				return;
			}

			recognizer_ = std::move(std::get<2>(newRecogOp));
		}
	}
}
#endif