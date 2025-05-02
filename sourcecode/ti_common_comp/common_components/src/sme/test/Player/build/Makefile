#all::debug release
all::sme debug 

sme :
	make -C ../../../build -f Makefile

#debug:: debugclean
debug:: 
	make -f Makefile.debug

debugclean::
	-make -f Makefile.debug clean

release:: releaseclean
	make -f Makefile.release

releaseclean::
	-make -f Makefile.release clean

clean:: debugclean releaseclean
