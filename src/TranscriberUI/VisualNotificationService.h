#pragma once
#include <QString>

namespace PticaGovorun
{
	class VisualNotificationService
	{
	public:
		virtual ~VisualNotificationService() { }

		virtual void nextNotification(const QString& message) = 0;
	};
}