
#ifndef _SPECTRUMPLUGIN_H
#define _SPECTRUMPLUGIN_H

#define PLUGIN_NAME "Mouse Spectrum Analyzer"
#define PLUGIN_AUTHOR "François Revol"

#define PALETTES_FOLDER "(MiniSpectrumPalettes)"
/* 14 + border color + fill color */
#define PAL_COLOR_COUNT 14
#define GRAPH_WIDTH 32
#define GRAPH_HEIGHT PAL_COLOR_COUNT
#define DISPLAY_WIDTH (GRAPH_WIDTH+2)
#define DISPLAY_HEIGHT (GRAPH_HEIGHT+2)
#define MAX_TITLE_LEN 300
#define VIEW_WIDTH (DISPLAY_WIDTH+MAX_TITLE_LEN)
#define VIEW_HEIGHT (DISPLAY_HEIGHT)

//#define OFFSET_FROM_MOUSE 24.0,24.0
#define OFFSET_FROM_MOUSE 14.0,23.0

#define EXP_INCREMENT 30
#define PHASE_NONE 0
#define PHASE_EXPANDING 1
#define PHASE_SHRINKING 2
#define PHASE_GROWN 3

#define MSG_SETCOL 'SetC'
#define MSG_SETTOOLTIP 'SetT'

#define PREF_PAL "MSA:Palette"
#define PREF_TT "MSA:ToolTip"

BView* configureplugin(BMessage *config);
void *getspectrumplugin(void **data,const char *name, const char *, uint32, plugin_info * );
void destroyspectrumplugin(void*,void *_loader);
status_t spectrumfilter(void *,short *buffer,int32 length, filter_info *info);
void spectrumfilechange(void *, const char *name, const char *path);

#endif
