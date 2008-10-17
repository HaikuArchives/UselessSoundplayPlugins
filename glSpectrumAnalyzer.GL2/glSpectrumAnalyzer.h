
#ifndef _GLSPECTRUMPLUGIN_H
#define _GLSPECTRUMPLUGIN_H

void *getspectrumplugin(void **data,const char *name, const char *, uint32, plugin_info * );
void destroyspectrumplugin(void*,void *_loader);
status_t spectrumfilter(void *,short *buffer,int32 length, filter_info *info);
void spectrumfilechange(void *, const char *name, const char *path);

#endif
