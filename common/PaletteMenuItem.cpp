/*
	Copyright 2003 François Revol (revol@free.fr)

	released under MIT Licence.
*/

#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include <Alert.h>
#include <Application.h>
#include <ByteOrder.h>
#include <Directory.h>
#include <Font.h>
#include <GraphicsDefs.h>
#include <Handler.h>
#include <MediaDefs.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <SupportDefs.h>
#include <StopWatch.h>
#include <TypeConstants.h>
#include <View.h>
#include <Window.h>

#include "PaletteMenuItem.h"

const rgb_color kDefaultPalette[DEFAULT_PAL_CNT] = {
{0, 0, 0, 255},//{0x9e, 0x9e, 0x9e, 255}, /* border color */
{0, 0, 0, 255},//{0x9e, 0x9e, 0x9e, 255}, /* fill color */
{255, 0, 0, 255},
{0, 0, 0, 255},//{0x9e, 0x9e, 0x9e, 255},
{0, 185, 0, 255},
{0, 225, 0, 255},
{0, 245, 0, 255},
{0, 255, 0, 255}
};


/* returns the number of colors, or an error code */
/* palette must be free()d by the caller when finished */
int32 LoadPalette(const char *name, rgb_color **palette)
{
	FILE *pfile;
	rgb_color *pal;
	char buffer[1024];
	int i, r, g, b;
	int count;
	if (!name)
		return EINVAL;
	pfile = fopen(name, "r");
	if (!pfile)
		return EIO;
	for (i = 0; (i < 500) && fgets(buffer, 1024, pfile);) {
		if (buffer[0] != '#')
			continue;
		if (sscanf(buffer, "#%02x%02x%02x", &r, &g, &b) > 2) {
			i++;
		}
	}
	count = i;
	if (!count)
		return 0;
	pal = (rgb_color *)malloc(count*sizeof(rgb_color));
	if (!pal)
		return B_NO_MEMORY;
	fseek(pfile, 0LL, SEEK_SET);
	for (i = 0; (i < count) && fgets(buffer, 1024, pfile);) {
		if (buffer[0] != '#')
			continue;
		if (sscanf(buffer, "#%02x%02x%02x", &r, &g, &b) > 2) {
			pal[i].red = r;
			pal[i].green = g;
			pal[i].blue = b;
			pal[i].alpha = 0xff;
			i++;
		}
	}
	*palette = pal;
	return count;
}

/* takes ownership of palette, free()s it in destructor */
PaletteMenuItem::PaletteMenuItem(rgb_color *palette, 
								int numcolors, 
								const char *label, 
								BMessage *message, 
								char shortcut, 
								uint32 modifiers)
	: BMenuItem(label, message, shortcut, modifiers)
{
	fColorCount = numcolors;
	fPalette = palette;
}

PaletteMenuItem::~PaletteMenuItem()
{
	if (fPalette)
		free(fPalette);
}

void PaletteMenuItem::DrawContent()
{
	int i;
	float w, h;
	GetContentSize(&w, &h);
	BRect colRect(Frame());
	colRect.top = colRect.bottom - 1;
	w = colRect.Width();
	colRect.right = colRect.left + w / fColorCount;
	if (fPalette) {
		rgb_color mcol = Menu()->HighColor();
		for (i = 0; i < fColorCount; i++)
		{
			Menu()->SetHighColor(fPalette[i]);
			Menu()->FillRect(colRect);
			colRect.OffsetBy(w / fColorCount, 0);
		}
		Menu()->SetHighColor(mcol);
	}
	BMenuItem::DrawContent();
}

const rgb_color *PaletteMenuItem::Palette(int *colors)
{
	if (fPalette)
		*colors = fColorCount;
	return fPalette;
}

