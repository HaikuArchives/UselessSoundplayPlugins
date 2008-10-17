#ifndef _PALETTE_MENU_ITEM_H
#define _PALETTE_MENU_ITEM_H
/*
	Copyright 2003 François Revol (revol@free.fr)
	
	released under MIT Licence.
*/


#include <GraphicsDefs.h>
#include <MenuItem.h>

/* returns the number of colors, or an error code */
/* palette must be free()d by the caller when finished */
extern int32 LoadPalette(const char *name, rgb_color **palette);

class PaletteMenuItem : public BMenuItem
{
public:
	PaletteMenuItem(rgb_color *palette, 
					int numcolors, 
					const char *label, 
					BMessage *message, 
					char shortcut = 0, 
					uint32 modifiers = 0);
virtual ~PaletteMenuItem();
virtual void DrawContent();
rgb_color *Palette(int *colors);
	
rgb_color *fPalette;
int fColorCount;
};

#endif /* _PALETTE_MENU_ITEM_H */