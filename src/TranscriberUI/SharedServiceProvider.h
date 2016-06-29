#pragma once
#include <memory>

namespace PticaGovorun
{
	class VisualNotificationService;
#ifdef PG_HAS_JULIUS
	class JuliusRecognizerProvider;
	class RecognizerNameHintProvider;
#endif

	class SharedServiceProvider
	{
	public:
		virtual std::shared_ptr<VisualNotificationService> notificationService() = 0;
		
#ifdef PG_HAS_JULIUS
		virtual std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider() = 0;
		virtual std::shared_ptr<RecognizerNameHintProvider> recognizerNameHintProvider() = 0;
#endif

	//protected:
		virtual ~SharedServiceProvider() { }
	};
}