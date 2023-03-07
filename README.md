
## hacked up linux (SDL) ports of very early mame releases

This began as an attempt to just learn the basics of how mame works by looking at very early source code [releases](https://www.mamedev.org/oldrel.html).  The source tree was *much* smaller (and thus more digestible) back then.  Modern mame is *way* more capable, but also (necessarily) more complicated, too.

It started with mame v0.1 (from 1997).  While messing with it, a simple interface based on SDL was created to replace the msdos source for graphics / sound.  All work on this was done in linux, but it likely will compile and run on other SDL compatible platforms, too).

The early versions of mame were rapidly changing in design with each new release - requiring frequent rework of the SDL interface.  A selection of random releases with working SDL implementations remain here (specifically v0.1, v0.9, 0.30, and v0.37b5).

As of now, it's stopped at version 0.37b5.  This is because:

	- it can emulate a fairly large number of games (looks like 2240 according to ./mame -listfull) already
	- it is apparently still widely used via retroarch on raspberry pis, etc (as "mame 2000")
	- it is reputed to be fairly lightweight and performant compared to later (more capable) versions of mame
	- it supports zipped roms/samples and includes checksum verifications (-verifyroms -verifysamples)
	- roms/samples compatible with this specific version can be easily found on the internet
	- later mame versions (0.106 on) already have an sdl interface available anyway (sdlmame)
	- mame has matured significantly since.  for anything more modern, probably best to just use the latest version

Disclaimer: As stated, this was just a learning experiment, not an actual attempt to "port" mame.  The effort was typically to get each version up and running ASAP and then move on to a later version.  As such, some functionality may not work properly (problematic things may have just been haphazardly commented out or removed, for example).  I'm not even sure exactly what functionality was/was not suppose to work back then (or what known bugs at the time may have been, etc).  So don't be shocked if your favorite command line argument doesn't do what you expect (nearly all of the msdos video modes stuff for example is not there).   That said, most games tested seemed to work at least.  And attempts were made to squash obvious bugs.

---

no roms are included with this repository.   roms (and samples) should be placed in the subfolders of "externals":

	oldroms / oldsamples are used by the earliest mame versions - and should contain the oldest versions of roms in "raw" unzipped format (one folder per game, one file per rom - and named as specified by mame).  see the README.txt files provided for specific lists or rom names.  the earliest mame versions only supported a few games anyway.

	oldsamples should also be placed in one folder per game, and should be in the old mame ".sam" sample format.   a python tool (externals/oldsamples/wav2sam.py) is provided to convert .wav files into .sam files.   by way of example, the oldsamples files for two games (dkong and asteroid) are included with this repository.

	roms_0.37b5 and samples_0.37b5 should contain just one zip file per game.

