/*---------------------------------------------------------------------------------

	$Id: main.cpp,v 1.13 2008-12-02 20:21:20 dovoto Exp $

	Using the debug console with no$gba
	-- dovoto, plateofshrimp


---------------------------------------------------------------------------------*/
#include <nds.h>

#include <stdio.h>

int main(void) {
	touchPosition touchXY;

	consoleDebugInit(DebugDevice_NOCASH);

	fprintf(stderr, "hello?\n");

	while(pmMainLoop()) {

		swiWaitForVBlank();
		scanKeys();
		int keys = keysDown();
		if (keys & KEY_START) break;

		touchRead(&touchXY);
	}

	return 0;
}
