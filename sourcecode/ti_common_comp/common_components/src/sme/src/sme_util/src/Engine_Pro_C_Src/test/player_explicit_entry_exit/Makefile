all::debug release

debug:: debugclean
	make -f Makefile.debug

debugclean::
	-make -f Makefile.debug clean

release:: releaseclean
	make -f Makefile.release

releaseclean::
	-make -f Makefile.release clean

clean:: debugclean releaseclean
