TitleSpectrumAnalyzer (TFFT) plugin for SoundPlay
copyright 2003, mmu_man, revol@free.fr

This plugins uses SoundPlay's title tab as an fft display.
Pure l33tness if you ask me :-)
Note: you must use SoundPlay in its native BeOS window to see any effect, it won't work in winamp mode (actually it will but you won't see anything because SoundPlay just hides its main window to display the winamp one).

The plugin configuration panel allows you to change the glyphs that will be used to render the fft.
The last couple of strings make it behave like a vu-meter.
You can also prefix the display with the program name, and postfix it with the soung title.
The Downsampling option lowers the number of redraws, which flickers less.

The .dano binary has some strings that only work with Dano-ish font overlay.

How to recompile:
you need the SoundPlay development kit.
Install it, and put this folder in the Plugin-dev/ folder, open a Terminal here, and type make.
The binary is in obj.x86/
I included a .proj file, though I didn't tested it.

Enjoy.

ChangeLog:
1.1: 05/13/2003
added before/after BBoxes, and checkbox to disable the fft display.

1.0: initial rev. 05/12/2003
