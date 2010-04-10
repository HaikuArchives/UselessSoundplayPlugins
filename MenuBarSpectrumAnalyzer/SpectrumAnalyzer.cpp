
/*
	MenuBar Spectrum Analyzer visualization plugin for SoundPlay.
	Copyright 2003 François Revol (revol@free.fr)
	Copyright 2001 Marco Nelissen (marcone@xs4all.nl)

	Permission is hereby granted to use this code for creating other
	SoundPlay-plugins.
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

#include "pluginproto.h"
#include "SpectrumAnalyzer.h"
#include "PaletteMenuItem.h"


plugin_descriptor plugin={
	PLUGIN_DESCRIPTOR_MAGIC,PLUGIN_DESCRIPTOR_VERSION,
	PLUGIN_NAME,1, PLUGIN_IS_VISUAL,
	PLUGIN_NAME,
	"By "PLUGIN_AUTHOR", Marco Nelissen.\n\n",
	NULL, // about
	NULL, //&configureplugin, // configure (if the plugin isn't running, options won't be saved for now...
	&getspectrumplugin,
	&destroyspectrumplugin
};

BView* filter_configureplugin(void *);
void SetConfig(void*, BMessage*);
void GetConfig(void*, BMessage*);

filter_plugin_ops ops={
	// first some bookkeeping stuff
	PLUGIN_VISUAL_MAGIC, PLUGIN_VISUAL_VERSION,

	// and the function pointers.
	spectrumfilechange,
	spectrumfilter,
	//filter_configureplugin, //NULL,
	NULL, //SetConfig,
	NULL, //GetConfig
	NULL,
	NULL
};

static plugin_descriptor *plugs[]={
	&plugin,
	0
};

plugin_descriptor **get_plugin_list(void) // this function gets exported
{
	return plugs;
}

// ==============================================================================

class MenuBarSpectrumAnalyzerPlugin : public BMenuItem
{
public:
	MenuBarSpectrumAnalyzerPlugin(plugin_info *info, BMenu *menu);
	virtual ~MenuBarSpectrumAnalyzerPlugin();
	void	Highlight(bool flag);
virtual void	GetContentSize(float *width, float *height);
virtual void	DrawContent();
status_t	Render(const short *buffer, int32 count);
	
	SoundPlayController *fController;
	entry_ref fPluginRef;
	BString fCurrentPalette;
	const rgb_color *fPalette;
	int fColors;
	bool fKeepRunning;
	bool fCheckSelection;
};

MenuBarSpectrumAnalyzerPlugin::MenuBarSpectrumAnalyzerPlugin(plugin_info *info, BMenu *menu)
	: BMenuItem(menu)
{
	fController = info->controller;
	fPluginRef = *(info->ref);
	fPalette = kDefaultPalette;
	fColors = DEFAULT_PAL_CNT;
	fKeepRunning = true;
	fCheckSelection = true;
	
	BMessage config;
	fController->RetrievePreference(PLUGIN_NAME, &config);
	fCurrentPalette = "Default";
	config.FindString(PREF_PAL, &fCurrentPalette);
	PaletteMenuItem *item = dynamic_cast<PaletteMenuItem *>(menu->FindItem(fCurrentPalette.String()));
	if (item) {
		fPalette = item->Palette(&fColors);
		if (!fPalette) {
			fPalette = kDefaultPalette;
			fColors = DEFAULT_PAL_CNT;
		} else {
			item->SetMarked(true);
		}
	}

}

MenuBarSpectrumAnalyzerPlugin::~MenuBarSpectrumAnalyzerPlugin()
{
	BMessage config;
	config.AddString(PREF_PAL, fCurrentPalette);
	fController->StorePreference(PLUGIN_NAME, &config);
}

/* since BMenuItem isn't a BView/BHandler, we can't get notifications in MessageReceived() */
void MenuBarSpectrumAnalyzerPlugin::Highlight(bool flag)
{
	BMenuItem::Highlight(flag);
	if (flag)
		return;
	fCheckSelection = true;
}

void MenuBarSpectrumAnalyzerPlugin::GetContentSize(float *width, float *height)
{
	*width = DISPLAY_WIDTH;
	*height = DISPLAY_HEIGHT;
}

void MenuBarSpectrumAnalyzerPlugin::DrawContent()
{
	/* draw nothing, the Render() method will do that when asked by SoundPlay */
	/* we don't call BMenuItem::DrawContent() because we won't want the label */
}

status_t MenuBarSpectrumAnalyzerPlugin::Render(const short *buf, int32 framecount)
{
	BMenu *smenu, *menu;
	bool locked;
	
	/* last time we decided to exit... do it this time */
	if (!fKeepRunning)
		return B_ERROR;
	menu = Menu();
	if (fCheckSelection) {
		bool slocked;
		
		fCheckSelection = false;
		/* should be safe that way... didn't crash without locking, but better play safe */
		smenu = Submenu();
		if (menu) {
			locked = menu->LockLooper();
			if (smenu && locked)
				slocked = smenu->LockLooper();
		}
		
		if (locked) {
			/* the menu has been released, so maybe a choice has been made */
			BMenuItem *item = smenu->FindMarked();
			if (item) {
				//fprintf(stderr, PLUGIN_NAME": Selected Item: %s\n", item->Label());
				PaletteMenuItem *pitem;
				pitem = dynamic_cast<PaletteMenuItem *>(item);
				if (!pitem)
					fKeepRunning = false; /* next time we'll exit */
				else {
					fColors = 0;
					int cols;
					fPalette = pitem->Palette(&cols);
					fColors = cols;
					fCurrentPalette.SetTo(pitem->Label());
				}
			}
		}
		if (locked) {
			if (slocked) {
				smenu->UnlockLooper();
			}
			menu->UnlockLooper();
		}
	}
	
	(void)framecount;
	const uint16 *fft = fController->GetFFTForBuffer(buf,SP_MONO_CHANNEL);

	if (!fPalette)
		return B_OK;
	if (!menu)
		return B_OK;
	locked = menu->LockLooper();
	if (!locked)
		return B_OK;
	BRect r(0, 0, DISPLAY_WIDTH-1, DISPLAY_HEIGHT-1);
	r.OffsetTo(ContentLocation());
	rgb_color col = menu->LowColor();
	menu->SetLowColor(fPalette[0]);
	menu->StrokeRect(r, B_SOLID_LOW);
	r.InsetBy(1,1);
	if (fColors>1)
		menu->SetLowColor(fPalette[1]);
	else
		menu->SetLowColor(0x9e, 0x9e, 0x9e);
	menu->FillRect(r, B_SOLID_LOW);
	menu->SetLowColor(col);
	
	const uint16 *offt=fft;

	for(int x=0;x<32;x++)
	{
		uint32 total=0;
		for(int i=0;i<((x<5)?2:(x/3)) && (fft < (offt+256));i++)
			total+=*fft++;
		total/=4*512-x*50;
		total/=4;
		if (total > GRAPH_HEIGHT)
			total = GRAPH_HEIGHT;
		menu->BeginLineArray(total+1);
		int y, z;
		for (z=0, y=total; y>0; y--, z++) {
			BPoint pt(r.left+x, r.bottom-y+1);
			menu->AddLine(pt, pt, fPalette[(z+2>=fColors)?(fColors-1):(z+2)]);
		}
		menu->EndLineArray();
	}
	menu->Flush();
	menu->UnlockLooper();
	return B_OK;
}

// ==============================================================================


BView* configureplugin(BMessage *config)
{
	return NULL;
}

BView* filter_configureplugin(void *)
{
	return NULL;
}

void *getspectrumplugin(void **data,const char *, const char *, uint32, plugin_info *info )
{
	BMenu *menu = NULL;
	BMenuItem *item;
	BDirectory dir;
	BDirectory palDir;
	BEntry ent(info->ref);

	if (ent.InitCheck() == B_OK) {
		if (ent.GetParent(&dir) == B_OK) {
			BPath palpath;
			while (!dir.IsRootDirectory()) {
				ent.SetTo(&dir, PALETTES_FOLDER, true);
				if (palDir.SetTo(&ent) == B_OK) {
					ent.GetPath(&palpath);
					fprintf(stderr, "palettes in '%s'.\n", palpath.Path());
					break;
				}
				if (dir.SetTo(&dir, "..") != B_OK)
					break;
			}
		}
	}
	menu = new BMenu("Palettes");
	menu->SetRadioMode(true);
	if (palDir.InitCheck() == B_OK) {
		palDir.Rewind();
		item = new BMenuItem("Quit Plugin", new BMessage('plop'));
		menu->AddItem(item);
		menu->AddItem(new BSeparatorItem);
		item = new BMenuItem("Palette:", NULL);
		item->SetEnabled(false);
		menu->AddItem(item);
		rgb_color *defPal = (rgb_color *)malloc(DEFAULT_PAL_CNT*sizeof(rgb_color));
		if (defPal) {
			memcpy(defPal, kDefaultPalette, DEFAULT_PAL_CNT*sizeof(rgb_color));
			item = new PaletteMenuItem(defPal, DEFAULT_PAL_CNT, "Default", new BMessage(MSG_SETCOL));
			menu->AddItem(item);
		}
		while (palDir.GetNextEntry(&ent) == B_OK) {
			rgb_color *pal = NULL;
			BPath palpath;
			if (ent.GetPath(&palpath) != B_OK)
				continue;
			//fprintf(stderr, PLUGIN_NAME": loading palette '%s'\n", palpath.Path());
			int32 count = LoadPalette(palpath.Path(), &pal);
			if (count < 1)
				continue;
			BString itemName(palpath.Leaf());
			itemName.IReplaceAll(".txt", "");
			//fprintf(stderr, PLUGIN_NAME": loaded palette '%s' has %d colors\n", itemName.String(), count);
			item = new PaletteMenuItem(pal, count, itemName.String(), new BMessage(MSG_SETCOL));
			menu->AddItem(item);
		}
	}

	MenuBarSpectrumAnalyzerPlugin *plugin=new MenuBarSpectrumAnalyzerPlugin(info, menu);
	BWindow *spWin;
	BMenu *spMenuBar;
	spWin = be_app->WindowAt(0);
	if (spWin) {
		spWin->Lock();
		spMenuBar = spWin->KeyMenuBar();
		if (spMenuBar) {
			if (!(spMenuBar->AddItem(plugin))) {
				fprintf(stderr, PLUGIN_NAME": cannot add plugin to the menubar !!!\n");
			}
		}
		spWin->Unlock();
	}
	
	*data=plugin;
	return &ops;
}

void destroyspectrumplugin(void*,void *data)
{
	MenuBarSpectrumAnalyzerPlugin *plugin=(MenuBarSpectrumAnalyzerPlugin *)data;
	BWindow *spWin;
	BMenu *spMenuBar;
	spWin = be_app->WindowAt(0);
	if (spWin) {
		spWin->Lock();
		spMenuBar = spWin->KeyMenuBar();
		if (spMenuBar) {
			if (!(spMenuBar->RemoveItem(plugin))) {
				fprintf(stderr, PLUGIN_NAME": cannot remove plugin from the menubar !!!\n");
			}
		}
		spWin->Unlock();
	}
	delete plugin;
}

void spectrumfilechange(void *data, const char *name, const char *path)
{
}

status_t spectrumfilter(void *data, short *buffer, int32 count, filter_info*)
{
	MenuBarSpectrumAnalyzerPlugin *plugin=(MenuBarSpectrumAnalyzerPlugin *)data;
	return plugin->Render(buffer, count);
}

void SetConfig(void *data, BMessage *config)
{
}

void GetConfig(void *data, BMessage *config)
{
}


