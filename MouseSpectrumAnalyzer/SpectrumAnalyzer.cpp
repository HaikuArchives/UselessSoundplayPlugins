
/*
	Mouse Spectrum Analyzer visualization plugin for SoundPlay.
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
#include <CheckBox.h>
#include <Directory.h>
#include <Font.h>
#include <GraphicsDefs.h>
#include <MediaDefs.h>
#include <Menu.h>
#include <MenuField.h>
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
	filter_configureplugin, //NULL,
	NULL, //SetConfig,
	NULL, //GetConfig
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

class MouseSpectrumAnalyzerPlugin : public BView
{
public:
	MouseSpectrumAnalyzerPlugin(plugin_info *info);
	virtual ~MouseSpectrumAnalyzerPlugin();
	void	TryLoadPalette(const char *name);
	void	Draw(BRect updateRect);
	void	Pulse();
	void	MessageReceived(BMessage *msg);
	void	SetTitle(const char *title);
status_t	Render(const short *buffer, int32 count);
	
	SoundPlayController *fController;
	entry_ref fPluginRef;
	BString fCurrentPalette;
	BString fPlayingTitle;
	rgb_color *fPalette;
	int fColors;
	bool fKeepRunning;
	bool fCheckSelection;
	int fExpandPhase;
	bool fShowToolTip;
};

MouseSpectrumAnalyzerPlugin::MouseSpectrumAnalyzerPlugin(plugin_info *info)
	: BView(BRect(0, 0, VIEW_WIDTH-1, VIEW_HEIGHT-1), "MouseSpectrumAnalyzerPlugin", B_FOLLOW_NONE, B_WILL_DRAW|B_PULSE_NEEDED|B_FULL_UPDATE_ON_RESIZE)
{
	fController = info->controller;
	fPluginRef = *(info->ref);
	fPalette = NULL;
	fColors = 0;
	fKeepRunning = true;
	fCheckSelection = true;
	fExpandPhase = PHASE_NONE;
	fShowToolTip = true;
	
	BMessage config;
	fController->RetrievePreference(PLUGIN_NAME, &config);
	fCurrentPalette = "Default";
	config.FindString(PREF_PAL, &fCurrentPalette);
	config.FindBool(PREF_TT, &fShowToolTip);
	TryLoadPalette(fCurrentPalette.String());
}

MouseSpectrumAnalyzerPlugin::~MouseSpectrumAnalyzerPlugin()
{
	BMessage config;
	config.AddString(PREF_PAL, fCurrentPalette);
	config.AddBool(PREF_TT, fShowToolTip);
	fController->StorePreference(PLUGIN_NAME, &config);
	if (fPalette)
		free(fPalette);
}

void MouseSpectrumAnalyzerPlugin::TryLoadPalette(const char *name)
{
	BString fname(name);
	/* cleanup */
	if (fPalette)
		free(fPalette);
	fPalette = NULL;
	fColors = 0;
	if (fname != "Default") {
		BDirectory dir;
		BEntry ent(&fPluginRef);
		if (ent.InitCheck() == B_OK) {
			if (ent.GetParent(&dir) == B_OK) {
				BPath palpath;
				ent.GetPath(&palpath);
				if (ent.SetTo(&dir, PALETTES_FOLDER, true) == B_OK) {
					if (dir.SetTo(&ent) == B_OK) {
						
					} else
						dir.Unset();
				}
			} else
				dir.Unset();
		}
		if (dir.InitCheck() == B_OK) {
			int cols;
			BPath palpath(&dir, fname.String());
			cols = LoadPalette(palpath.Path(), &fPalette);
			if (cols < 1) {
				fname << ".txt";
				palpath.SetTo(&dir, fname.String());
				cols = LoadPalette(palpath.Path(), &fPalette);
			}
			if (cols > 0)
				fColors = cols;
		}
	}
	if (!fColors) {
		fPalette = (rgb_color *)malloc(DEFAULT_PAL_CNT*sizeof(rgb_color));
		memcpy(fPalette, kDefaultPalette, DEFAULT_PAL_CNT*sizeof(rgb_color));
		fColors = DEFAULT_PAL_CNT;
		fCurrentPalette = "Default";
	} else
		fCurrentPalette = name;
	rgb_color l = {240, 240, 100, 255};
	if ((fColors > 1) && (*(uint32 *)&fPalette[0] != *(uint32 *)&fPalette[1])) {
		l = fPalette[1];
	}
	SetViewColor(l);
}

void MouseSpectrumAnalyzerPlugin::Draw(BRect updateRect)
{
	rgb_color l = {240, 240, 100, 255};
	rgb_color h = {0, 0, 0, 255};
	if (updateRect.right < DISPLAY_WIDTH)
		return; // not our business
	//l = ui_color(B_PANEL_BACKGROUND_COLOR);
	BRect r(Window()->Bounds()); /* might be shrinking or growing */
	//r.left = DISPLAY_WIDTH; /* don't clober the vis area */
	if ((fColors > 1) && (*(uint32 *)&fPalette[0] != *(uint32 *)&fPalette[1])) {
		l = fPalette[1];
		h = fPalette[0];
	}
	SetLowColor(l);
	SetHighColor(h);
	BFont f;
	font_height fh;
	float maxlen = MAX_TITLE_LEN-10;
	maxlen = f.StringWidth(fPlayingTitle.String(), maxlen);
	//printf("len=%f, %s\n", maxlen, fPlayingTitle.String());
	GetFont(&f);
	f.GetHeight(&fh);
//	FillRect(r, B_SOLID_LOW);
	StrokeRect(r);
	DrawString(fPlayingTitle.String(), maxlen-5, r.LeftBottom()+BPoint(3+DISPLAY_WIDTH, -(VIEW_HEIGHT-(fh.ascent+fh.descent+fh.leading))/2-2));
}

void MouseSpectrumAnalyzerPlugin::Pulse()
{
	if (!Window())
		return;
	/* follow the mouse */
	BPoint mouse;
	uint32 buttons;
	GetMouse(&mouse, &buttons);
	ConvertToScreen(&mouse);
	mouse += BPoint(OFFSET_FROM_MOUSE);
	Window()->MoveTo(mouse);
	/* handle shrink/grow */
	if (fExpandPhase == PHASE_EXPANDING) {
		BFont f;
		GetFont(&f);
		BRect r = Window()->Frame();
		r.right += EXP_INCREMENT;
		float maxlen = f.StringWidth(fPlayingTitle.String(), MAX_TITLE_LEN-10);
		maxlen = MIN(maxlen, MAX_TITLE_LEN-10);
		maxlen += DISPLAY_WIDTH+10;
		if (r.Width() >= maxlen) {
			r.right = r.left + maxlen - 1;
			fExpandPhase = PHASE_GROWN+10;
		}
		Window()->ResizeTo(r.Width(), r.Height());
	} else if (fExpandPhase >= PHASE_GROWN) {
		fExpandPhase--;
	} else if (fExpandPhase == PHASE_SHRINKING) {
		BRect r = Window()->Frame();
		r.right -= EXP_INCREMENT;
		if (r.Width() <= DISPLAY_WIDTH) {
			r.right = r.left + DISPLAY_WIDTH - 1;
			fExpandPhase = PHASE_NONE;
		}
		Window()->ResizeTo(r.Width(), r.Height());
	}
}

void MouseSpectrumAnalyzerPlugin::MessageReceived(BMessage *msg)
{
	const char *str;
	int cnum;
	rgb_color *palette;
	int32 doShowTT;
	switch (msg->what) {
	case MSG_SETCOL:
		if (msg->FindString("palette", &str) != B_OK)
			str = "Default";
		TryLoadPalette(str);
		break;
	case MSG_SETTOOLTIP:
		if (msg->FindInt32("be:value", &doShowTT) == B_OK)
			fShowToolTip = !!doShowTT;
		break;
	default:
		BView::MessageReceived(msg);
	}
}


void MouseSpectrumAnalyzerPlugin::SetTitle(const char *title)
{
	fPlayingTitle.SetTo(title);
	if (fShowToolTip)
		fExpandPhase = PHASE_EXPANDING;
}

status_t MouseSpectrumAnalyzerPlugin::Render(const short *buf, int32 framecount)
{
	bool locked;
	
	/* last time we decided to exit... do it this time */
	if (!fKeepRunning)
		return B_ERROR;
	
	(void)framecount;
	const uint16 *fft = fController->GetFFTForBuffer(buf,SP_MONO_CHANNEL);

	if (!fPalette)
		return B_OK;
	locked = LockLooper();
	if (!locked)
		return B_OK;
	BRect r(0, 0, DISPLAY_WIDTH-1, DISPLAY_HEIGHT-1);
	//r.OffsetTo(ContentLocation());
	rgb_color col = LowColor();

	//Window()->BeginViewTransaction();

	SetLowColor(fPalette[0]);
	StrokeRect(r, B_SOLID_LOW);
	r.InsetBy(1,1);
	if (fColors>1)
		SetLowColor(fPalette[1]);
	else
		SetLowColor(0x9e, 0x9e, 0x9e);
	FillRect(r, B_SOLID_LOW);
	SetLowColor(col);
	
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
		BeginLineArray(total+1);
		int y, z;
		for (z=0, y=total; y>0; y--, z++) {
			BPoint pt(r.left+x, r.bottom-y+1);
			AddLine(pt, pt, fPalette[(z+2>=fColors)?(fColors-1):(z+2)]);
		}
		EndLineArray();
	}

	Flush();

	//Window()->EndViewTransaction();

	UnlockLooper();
	return B_OK;
}

// ==============================================================================

class PrefsView : public BView
{
public:
	PrefsView(MouseSpectrumAnalyzerPlugin *plugin);
	~PrefsView();
void	AttachedToWindow();
void	MessageReceived(BMessage *msg);
MouseSpectrumAnalyzerPlugin *fPlugin;
};

PrefsView::PrefsView(MouseSpectrumAnalyzerPlugin *plugin)
	: BView(BRect(0,0,200,150), PLUGIN_NAME" Prefs", B_FOLLOW_NONE, 0)
{
	fPlugin = plugin;
}

PrefsView::~PrefsView()
{
}

void PrefsView::AttachedToWindow()
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	BMenuField *mf;
	BMenu *menu;
	BMenuItem *item;
	BDirectory dir;
	BEntry ent(&fPlugin->fPluginRef);
	if (ent.InitCheck() == B_OK) {
		if (ent.GetParent(&dir) == B_OK) {
			BPath palpath;
			while (ent.SetTo(&dir, PALETTES_FOLDER, true) == B_OK) {
				if (dir.SetTo(&ent) == B_OK) {
					ent.GetPath(&palpath);
					fprintf(stderr, "palettes in '%s'.\n", palpath.Path());
					break;
				}
				if (dir.IsRootDirectory()) {
					dir.Unset();
					break;
				}
				dir.SetTo(&dir, "..");
			}
		} else
			dir.Unset();
	}
	menu = new BMenu("Palette");
	menu->SetRadioMode(true);
	if (dir.InitCheck() == B_OK) {
		dir.Rewind();
		rgb_color *defPal = (rgb_color *)malloc(DEFAULT_PAL_CNT*sizeof(rgb_color));
		if (defPal) {
			memcpy(defPal, kDefaultPalette, DEFAULT_PAL_CNT*sizeof(rgb_color));
			BMessage *msg = new BMessage(MSG_SETCOL);
			item = new PaletteMenuItem(defPal, DEFAULT_PAL_CNT, "Default", msg);
			menu->AddItem(item);
		}
		while (dir.GetNextEntry(&ent) == B_OK) {
			rgb_color *pal = NULL;
			BPath palpath;
			if (ent.GetPath(&palpath) != B_OK)
				continue;
			fprintf(stderr, PLUGIN_NAME": loading palette '%s'\n", palpath.Path());
			int32 count = LoadPalette(palpath.Path(), &pal);
			if (count < 1)
				continue;
			BString itemName(palpath.Leaf());
			itemName.IReplaceAll(".txt", "");
			//fprintf(stderr, PLUGIN_NAME": loaded palette '%s' has %d colors\n", itemName.String(), count);
			BMessage *msg = new BMessage(MSG_SETCOL);
			msg->AddString("palette", itemName.String());
			item = new PaletteMenuItem(pal, count, itemName.String(), msg);
			if (itemName == fPlugin->fCurrentPalette)
				item->SetMarked(true);
			menu->AddItem(item);
		}
	}
	menu->SetTargetForItems(this);
	menu->SetLabelFromMarked(true);
	mf = new BMenuField(BRect(10, 10, 180, 29), "palette", "Palette: ", menu);
	AddChild(mf);
	BCheckBox *cb;
	cb = new BCheckBox(BRect(10, 30, 180, 49), "useTT", "Show tooltip on song change", new BMessage(MSG_SETTOOLTIP));
	AddChild(cb);
	cb->SetValue(fPlugin->fShowToolTip);
	cb->SetTarget(this);
}

void PrefsView::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
	case MSG_SETCOL:
	case MSG_SETTOOLTIP:
	{
		BMessenger msgr(fPlugin);
		BMessage *msg2 = new BMessage(*msg);
		msgr.SendMessage(msg2);
		break;
	}
	default:
		BView::MessageReceived(msg);
	}
}


// ==============================================================================


BView* configureplugin(BMessage *config)
{
	return NULL;
}

BView* filter_configureplugin(void *data)
{
	MouseSpectrumAnalyzerPlugin *plugin=(MouseSpectrumAnalyzerPlugin *)data;
	return new PrefsView(plugin);
}

void *getspectrumplugin(void **data,const char *, const char *, uint32, plugin_info *info )
{
	MouseSpectrumAnalyzerPlugin *plugin=new MouseSpectrumAnalyzerPlugin(info);
	BWindow *win;
	win = new BWindow(BRect(0, 0, DISPLAY_WIDTH-1, VIEW_HEIGHT-1).OffsetByCopy(10,10), 
					PLUGIN_NAME" :-)", 
					B_NO_BORDER_WINDOW_LOOK, 
					B_FLOATING_ALL_WINDOW_FEEL, 
					B_NOT_CLOSABLE|B_NOT_ZOOMABLE|B_NOT_MINIMIZABLE|B_AVOID_FOCUS, 
					B_ALL_WORKSPACES);
	win->AddChild(plugin);
	win->SetPulseRate(100000);
	win->Show();
	*data=plugin;
	return &ops;
}

void destroyspectrumplugin(void*,void *data)
{
	MouseSpectrumAnalyzerPlugin *plugin=(MouseSpectrumAnalyzerPlugin *)data;
	if (plugin->LockLooper())
		plugin->Window()->Quit();
	//delete plugin;
}

void spectrumfilechange(void *data, const char *name, const char *path)
{
	MouseSpectrumAnalyzerPlugin *plugin=(MouseSpectrumAnalyzerPlugin *)data;
	if (plugin->LockLooper()) {
		plugin->SetTitle(name);
		plugin->UnlockLooper();
	}
	return;
}

status_t spectrumfilter(void *data, short *buffer, int32 count, filter_info*)
{
	status_t err;
	MouseSpectrumAnalyzerPlugin *plugin=(MouseSpectrumAnalyzerPlugin *)data;
	if (plugin->LockLooper()) {
		err = plugin->Render(buffer, count);
		plugin->UnlockLooper();
	}
	return err;
}

void SetConfig(void *data, BMessage *config)
{
}

void GetConfig(void *data, BMessage *config)
{
}


