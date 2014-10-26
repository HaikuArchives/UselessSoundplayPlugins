UselessSoundplayPlugins
=======================

Some SoundPlay visualization plugins.

## TitleSpectrumAnalyzer (TFFT) plugin for SoundPlay
copyright 2003, mmu_man, revol@free.fr

This plugins uses SoundPlay's title tab as an fft display.
Pure l33tness if you ask me :-)
Note: you must use SoundPlay in its native BeOS window to see any effect, it won't work in winamp mode (actually it will but you won't see anything because SoundPlay just hides its main window to display the winamp one).

The plugin configuration panel allows you to change the glyphs that will be used to render the fft.
The last couple of strings make it behave like a vu-meter.
You can also prefix the display with the program name, and postfix it with the soung title.
The Downsampling option lowers the number of redraws, which flickers less.

The .dano binary has some strings that only work with Dano-ish font overlay.

## MouseSpectrumAnalyzer

An FFT following the mouse as a tooltip.

## MenuBarSpectrumAnalyzer

Adds a little menu entry SoundPlay's window with an FFT view.

## glSpectrumAnalyzer.GL1

A 3D spectrum analyser based on NeHe tutorials and the spectrum analyser SoundPlay sample code.

## glSpectrumAnalyzer.GL2

Same as glSpectrumAnalyzer.GL1 but for the Dano GL Kit.
