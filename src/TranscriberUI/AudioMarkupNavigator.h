#pragma once

class AudioMarkupNavigator
{
public:
	AudioMarkupNavigator();
	virtual ~AudioMarkupNavigator();

public:
	virtual bool requestMarkerId(int& markerId);
};

