SUBMAKEFILESPRE=
SUBDIRS= MouseSpectrumAnalyzer glSpectrumAnalyzer.GL1 \
MenuBarSpectrumAnalyzer TitleSpectrumAnalyzer glSpectrumAnalyzer.GL2
SUBMAKEFILESPOST=

default all clean:
	for f in $(SUBMAKEFILESPRE); do \
		make -f $$f $@ || exit 1; \
	done
	for d in $(SUBDIRS); do \
		make -C $$d $@ || exit 1; \
	done
	for f in $(SUBMAKEFILESPOST); do \
		make -f $$f $@ || exit 1; \
	done


