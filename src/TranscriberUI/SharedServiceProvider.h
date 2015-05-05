#pragma once
#include <memory>

namespace PticaGovorun
{
	class VisualNotificationService;
	class JuliusRecognizerProvider;
	class RecognizerNameHintProvider;

	class SharedServiceProvider
	{
	public:
		virtual std::shared_ptr<VisualNotificationService> notificationService() = 0;
		
		virtual std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider() = 0;

		virtual std::shared_ptr<RecognizerNameHintProvider> recognizerNameHintProvider() = 0;

	//protected:
		virtual ~SharedServiceProvider() { }
	};
}