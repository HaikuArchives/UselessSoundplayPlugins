
#ifndef _SPECTRUMPLUGIN_H
#define _SPECTRUMPLUGIN_H

//#define PLUGIN_NAME "MenuBarAnalyzer"
#define PLUGIN_NAME "MenuBar Spectrum Analyzer"
#define PLUGIN_AUTHOR "François Revol"

/* 14 + border color + fill color */
#define PAL_COLOR_COUNT 14
#define GRAPH_WIDTH 32
#define GRAPH_HEIGHT PAL_COLOR_COUNT
#define DISPLAY_WIDTH (GRAPH_WIDTH+2)
#define DISPLAY_HEIGHT (GRAPH_HEIGHT+2)

#define MSG_SETCOL 'SetC'

#define PREF_PAL "BMSA:Palette"

BView* configureplugin(BMessage *config);
void *getspectrumplugin(void **data,const char *name, const char *, uint32, plugin_info * );
void destroyspectrumplugin(void*,void *_loader);
status_t spectrumfilter(void *,short *buffer,int32 length, filter_info *info);
void spectrumfilechange(void *, const char *name, const char *path);

#endif
