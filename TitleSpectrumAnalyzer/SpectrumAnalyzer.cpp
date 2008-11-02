
/*
	Title bar Spectrum Analyzer visualization plugin for SoundPlay.
	Copyright 2003 Fran√ßois Revol (revol@free.fr)
	Copyright 2001 Marco Nelissen (marcone@xs4all.nl)

	Permission is hereby granted to use this code for creating other
	SoundPlay-plugins.
*/

#include <Application.h>
#include <SupportDefs.h>
#include <TypeConstants.h>
#include <ByteOrder.h>
#include <string.h>
#include <stdio.h>
#include <MediaDefs.h>
//#include <Alert.h>
#include <Box.h>
#include <CheckBox.h>
#include <RadioButton.h>

#include <View.h>
#include <Window.h>
#include <malloc.h>
#include <StopWatch.h>
#include <Font.h>
#include "pluginproto.h"
#include "SpectrumAnalyzer.h"

#define MAX_BARGRAPH_GLYPHS 30
#define MAX_GRAPHS 22
#define MAX_CHARS 4

/* configuration, global-C-var-style (yeah, kids, don't do that at home !) */

int32 current_graph = 0;
bool display_soundplay_before = true;
bool display_title_before = false;
bool display_soundplay_after = false;
bool display_title_after = false;
bool display_fft = true;
bool force_refresh = false;
int32 down_sample = 0;


int32 graph_count = 0;
int32 graph_len[MAX_GRAPHS];
int32 this_round = 0; /* for down_sample */

const char *graphs[][MAX_BARGRAPH_GLYPHS] = {
{ " ", ".", "‡Ñ±", "‡Ç°", /*"‡Å©", */"‡Åâ", "‡Ñ∞" }, // PERFECT with the default R5 font !
//" ", 
{ " ", ".", "‡Ç∑", "‡Ç°", "‡Å©", "‡Åâ" },
// cedilla too wide // " ", "‡Ç∏", ",", "‡Ç°", "‡Å©", "‡Åâ"
{ " ", ".", "‡Ä∫", "‡Ç°", "‡Å©", "‡Åâ" },
{ " ", ".", "‡Ç∑", "‡Äß" },
{ " ", ".", "‡Ç∑"/*"‡Ç≠"*/, "‡ÇØ" },
{ "  ", "_", "-", "‡ãú", "‡ÇØ" },
{ "  ", "_", "--", "'''"},
//{ "  ", "_" "o", "¬∞" },
{ "-", "‡Åú", "|", "/" }, 
//{ "‡Åü", "‡Åæ", "‡Ç¨", "‡É∑", "‡ÄΩ" },
{ "‡Åü", "‚Äì", "‡Åø" },
//{ " ", "‡Ç∑", "‚Ä¢"  }, 
{ "S", "o", "u", "n", "d", "p", "l", "a", "y" },
{ "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" },
{ "a", "b", "c", "d", "e" },
{ "‡Ç∞", "‡Çπ", "‡Ç≤", "‡Ç≥" },
{ "    ", "‡Çº", "‡ÇΩ", "‡Çæ" },
#if 1//def B_BEOS_VERSION_DANO
// Dano handles Unicode overlays better...
//"  ", 
// UTF8 chars 9601 to 9608
{ "‚ñÅ", "‚ñÇ", "‚ñÉ", "‚ñÑ", "‚ñÖ", "‚ñÜ", "‚ñá", "‚ñà" },

// UTF8 chars 8592 to 8601
{ "‚Üô", "‚Üê", "‚Üñ", "‚Üë", "‚Üó", "‚Üí", "‚Üò" },
{ "‚Üó", "‚Üí", "‚Üò", "‚Üì", "‚Üô", "‚Üê", "‚Üñ" },
#endif
/* vu-meter mode */
{ "", "l", "l" }, 
{ "", "‡Ç¶", "‡Ç¶" }, 
{ "", "|", "|" }, 
{ "", "‡Åø", "‡Åø" }, 
{ "", " ", "  " }, 

{ NULL } 
};

#define MAX_BARGRAPH (sizeof(graph_levels)/sizeof(const char *))

vint32 refcount = 0;

plugin_descriptor plugin={
	PLUGIN_DESCRIPTOR_MAGIC,PLUGIN_DESCRIPTOR_VERSION,
	"TitleSpectrumAnalyzer",1, PLUGIN_IS_VISUAL,
	"TitleSpectrumAnalyzer",
	"By Fran√ßois Revol, Marco Nelissen.\n\n",
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
};

static plugin_descriptor *plugs[]={
	&plugin,
	0
};

plugin_descriptor **get_plugin_list(void) // this function gets exported
{
	// first count the available graphs, and the length of each
	for (graph_count = 0; graph_count < MAX_GRAPHS && graphs[graph_count][0]; graph_count++)
		for (graph_len[graph_count] = 0; graphs[graph_count][graph_len[graph_count]]; graph_len[graph_count]++);
	return plugs;
}


class TitleSpectrumAnalyzerPlugin
{
	private:
		char *name;
		char prev_buffer[MAX_BARGRAPH_GLYPHS*MAX_CHARS+5];
		char buffer[MAX_BARGRAPH_GLYPHS*MAX_CHARS+5];

	public:
		SoundPlayController *controller;
		TitleSpectrumAnalyzerPlugin(SoundPlayController *ctrl);
		virtual ~TitleSpectrumAnalyzerPlugin();
		void SetName(const char *name);
		status_t Render(const short *buffer, int32 count);
};

class TFFTConfigView : public BView
{
public:
	TFFTConfigView(BMessage *config);
	~TFFTConfigView();
	void MessageReceived(BMessage *message);
	void AttachedToWindow(void);
private:
	BMessage *fConfig;
	BRadioButton *graphsRadio[MAX_GRAPHS];
	BCheckBox *showSPBefore;
	BCheckBox *showTitleBefore;
	BCheckBox *showFFT;
	BCheckBox *showSPAfter;
	BCheckBox *showTitleAfter;
	BCheckBox *downSample;
};

TFFTConfigView::TFFTConfigView(BMessage *config)
	: BView(BRect(0, 0, 250, 10*MAX_GRAPHS+160), "TitleFFT plugin", B_FOLLOW_ALL_SIDES, B_WILL_DRAW)
{
	float curY = 15;
	fConfig = config;
//	GetConfig(NULL, fConfig);
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	//
	BBox *beforeBox = new BBox(BRect(15, curY, 250, curY + 35));
	beforeBox->SetLabel("Before");
	AddChild(beforeBox);
	curY += 40;
	showSPBefore = new BCheckBox(BRect(5, 11, 115, 25), "showSPBefore", "Show 'SoundPlay'", new BMessage('updt'));
	showSPBefore->SetValue(display_soundplay_before);
	beforeBox->AddChild(showSPBefore);
	showTitleBefore = new BCheckBox(BRect(115, 11, 225, 25), "showTitleBefore", "Show Song Title", new BMessage('updt'));
	showTitleBefore->SetValue(display_title_before);
	beforeBox->AddChild(showTitleBefore);
	//
	BBox *graphsBox = new BBox(BRect(15, curY, 250, curY + 10*MAX_GRAPHS+20));
	showFFT = new BCheckBox(BRect(0, 0, 25, 20), "showFFT", "Bargraph", new BMessage('updt'));
	showFFT->SetValue(display_fft);
	BFont font;
	beforeBox->GetFont(&font);
	showFFT->SetFont(&font);
	graphsBox->SetLabel(showFFT);
	AddChild(graphsBox);
	curY += 10*MAX_GRAPHS+25;
	for (int i = 0; i < MIN(graph_count, MAX_GRAPHS); i++) {
		char buffer[MAX_BARGRAPH_GLYPHS*MAX_CHARS+5];
		buffer[0] = '\0';
		for (int j = 0; j < graph_len[i]; j++)
			strcat(buffer, graphs[i][j]);
		graphsRadio[i] = new BRadioButton(((i < (MAX_GRAPHS/2))?
													(BRect(10, 18 + 20*i, 100, 38 + 20*i)):
													(BRect(110, 18 + 20*(i-(MAX_GRAPHS/2)), 210, 38 + 20*(i-(MAX_GRAPHS/2))))), 
											"radio", buffer, new BMessage('updt'));
		if (i == current_graph)
			graphsRadio[i]->SetValue(B_CONTROL_ON);
		graphsBox->AddChild(graphsRadio[i]);
	}
	//
	BBox *afterBox = new BBox(BRect(15, curY, 250, curY + 35));
	afterBox->SetLabel("After");
	AddChild(afterBox);
	curY += 40;
	showSPAfter = new BCheckBox(BRect(5, 11, 115, 25), "showSP", "Show 'SoundPlay'", new BMessage('updt'));
	showSPAfter->SetValue(display_soundplay_after);
	afterBox->AddChild(showSPAfter);
	showTitleAfter = new BCheckBox(BRect(115, 11, 225, 25), "showTitle", "Show Song Title", new BMessage('updt'));
	showTitleAfter->SetValue(display_title_after);
	afterBox->AddChild(showTitleAfter);
	//
	downSample = new BCheckBox(BRect(15, curY, 235, curY+20), "downSample", "Downsample drawing (flickers less)", new BMessage('updt'));
	downSample->SetValue(down_sample);
	AddChild(downSample);
}

TFFTConfigView::~TFFTConfigView(void)
{
//	SetConfig(NULL, fConfig);
}

void TFFTConfigView::AttachedToWindow(void)
{
	for (int i = 0; i < graph_count && graphsRadio[i]; i++)
		graphsRadio[i]->SetTarget(this);
	showSPBefore->SetTarget(this);
	showTitleBefore->SetTarget(this);
	showSPAfter->SetTarget(this);
	showTitleAfter->SetTarget(this);
	showFFT->SetTarget(this);
	downSample->SetTarget(this);
}

void TFFTConfigView::MessageReceived(BMessage *message)
{
	BView::MessageReceived(message);
	if (message->what != 'updt')
		return;
	int current = 0;
	for (int i = 0; i < graph_count; i++)
		if (graphsRadio[i] && graphsRadio[i]->Value())
			current_graph = i;
	display_soundplay_before = showSPBefore->Value();
	display_title_before = showTitleBefore->Value();
	display_soundplay_after = showSPAfter->Value();
	display_title_after = showTitleAfter->Value();
	display_fft = showFFT->Value();
	down_sample = (int32) downSample->Value();
	force_refresh = true;
}

BView* configureplugin(BMessage *config)
{
	return new TFFTConfigView(config);
}

BView* filter_configureplugin(void *)
{
	return new TFFTConfigView(NULL);
}

void *getspectrumplugin(void **data,const char *, const char *, uint32, plugin_info *info )
{
	if (refcount) // allow only one instance
		return NULL;
	refcount++;
	BMessage config;
	info->controller->RetrievePreference("TitleSpectrumAnalyzer", &config);
	GetConfig(NULL, &config);
	TitleSpectrumAnalyzerPlugin *plugin=new TitleSpectrumAnalyzerPlugin(info->controller);
	*data=plugin;
	return &ops;
}

void destroyspectrumplugin(void*,void *data)
{
	TitleSpectrumAnalyzerPlugin *plugin=(TitleSpectrumAnalyzerPlugin *)data;
	delete plugin;
	if (be_app->WindowAt(0)) {
		be_app->WindowAt(0)->Lock();
		be_app->WindowAt(0)->SetTitle("SoundPlay");
		be_app->WindowAt(0)->Unlock();
	}
	BMessage config;
	SetConfig(NULL, &config);
	plugin->controller->StorePreference("TitleSpectrumAnalyzer", &config);
	refcount--;
}

void spectrumfilechange(void *data, const char *name, const char *path)
{
	TitleSpectrumAnalyzerPlugin *plugin=(TitleSpectrumAnalyzerPlugin *)data;
	plugin->SetName(name);
}

status_t spectrumfilter(void *data, short *buffer, int32 count, filter_info*)
{
	TitleSpectrumAnalyzerPlugin *plugin=(TitleSpectrumAnalyzerPlugin *)data;
	return plugin->Render(buffer, count);
}

void SetConfig(void *data, BMessage *config)
{
	config->AddInt32("tfft:graph", current_graph);
	config->AddBool("tfft:showspb4", display_soundplay_before);
	config->AddBool("tfft:showttlb4", display_title_before);
	config->AddBool("tfft:showspaf", display_soundplay_after);
	config->AddBool("tfft:showttlaf", display_title_after);
	config->AddBool("tfft:showfft", display_fft);
	config->AddInt32("tfft:down_sample", down_sample);
}

void GetConfig(void *data, BMessage *config)
{
	config->FindInt32("tfft:graph", &current_graph);
	config->FindBool("tfft:showspb4", &display_soundplay_before);
	config->FindBool("tfft:showttlb4", &display_title_before);
	config->FindBool("tfft:showspaf", &display_soundplay_after);
	config->FindBool("tfft:showttlaf", &display_title_after);
	config->FindBool("tfft:showfft", &display_fft);
	config->FindInt32("tfft:down_sample", &down_sample);
}


// ==============================================================================
TitleSpectrumAnalyzerPlugin::TitleSpectrumAnalyzerPlugin(SoundPlayController *ctrl)
	:name(NULL)
{
	controller=ctrl;
	prev_buffer[0] = '\0';
}

TitleSpectrumAnalyzerPlugin::~TitleSpectrumAnalyzerPlugin()
{
	if (name)
		free(name);
}

void TitleSpectrumAnalyzerPlugin::SetName(const char *name)
{
	if (this->name)
		free(this->name);
	this->name = strdup(name);
	force_refresh = true;
}

status_t TitleSpectrumAnalyzerPlugin::Render(const short *buf, int32 framecount)
{
	bool need_dash = false;
	this_round ++;
	this_round %= down_sample * 3 + 1;
	if (this_round && !force_refresh)
		return B_OK; /* skip drawing */
	
	BString title;
	buffer[0] = '\0';
	
	if (display_fft) {
		const uint16 *fft = controller->GetFFTForBuffer(buf,SP_MONO_CHANNEL);

		for(int x=0;x<20;x++)
		{
			uint32 total=0;
			for(int i=0;i<3+x;i++)
				total+=*fft++;
			total/=4*512-x*90;
			if(total>110)
				total=110;

			strcat(buffer, graphs[current_graph][((MIN(total, 125))*(graph_len[current_graph])/125)]);
		}
	}
	if (!force_refresh && !strcmp(buffer, prev_buffer))
		return B_OK; // avoid a trip to app_server to finally not change name
	force_refresh = false;
	strcpy(prev_buffer, buffer);
	
	if (display_soundplay_before) {
		title << "SoundPlay";
		need_dash = true;
	}
	if (display_title_before) {
		if (need_dash) {
			need_dash = false;
			title << " - ";
		}
		title << name;
		need_dash = true;
	}
	if (display_fft) {
		if (need_dash) {
			need_dash = false;
			title << " - ";
		}
		title << buffer;
		need_dash = true;
	}
	if (display_soundplay_after) {
		if (need_dash) {
			need_dash = false;
			title << " - ";
		}
		title << "SoundPlay";
		need_dash = true;
	}
	if (display_title_after) {
		if (need_dash) {
			need_dash = false;
			title << " - ";
		}
		title << name;
		need_dash = true;
	}
	
	if (be_app->WindowAt(0)) {
		be_app->WindowAt(0)->Lock();
		be_app->WindowAt(0)->SetTitle(title.String());
		be_app->WindowAt(0)->Unlock();
	}

	return B_OK;
}
