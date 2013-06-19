#include <stdio.h>
#include <string.h>

#include "configstream.h"

ConfigStream::ConfigStream(void)
{

}

void ConfigStream::goToHomeScreen(void)
{
	ConfigComponent *comp;
	
	comp = new ConfigComponent(ConfigComponent::label, "Home Screen", 12, 0, 0);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::checkbox, "", 3, 0, 1);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::editline, "", 16, 0, 2);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " OK ", 4, 0, 3);
	screen.push_back(comp);
}

void ConfigStream::getStream(char *bfr, int maxLen)
{
	memset(bfr, 0, maxLen);

	for(int i=0; i<screen.size(); i++) {
		ConfigComponent *c = screen[i];
		
		int gotLen;
		c->getStream(true, bfr, gotLen);
		bfr += gotLen;
	}
}

