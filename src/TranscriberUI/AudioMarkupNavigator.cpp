#include "AudioMarkupNavigator.h"
#include "AudioMarkupNavigatorDialog.h"


AudioMarkupNavigator::AudioMarkupNavigator()
{
}


AudioMarkupNavigator::~AudioMarkupNavigator()
{
}

bool AudioMarkupNavigator::requestMarkerId(int& markerId)
{
	AudioMarkupNavigatorDialog audioNavigatorDlg;
	if (audioNavigatorDlg.exec() == QDialog::Accepted)
	{
		markerId = audioNavigatorDlg.markerId();
		return true;
	}

	return false;
}