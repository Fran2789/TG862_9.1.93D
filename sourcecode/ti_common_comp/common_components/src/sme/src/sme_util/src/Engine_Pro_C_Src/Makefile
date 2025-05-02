
PROJECT_ROOT=.

all::debug release

#########################################################
# Debug version
#########################################################

OUTPUT_DIR_DEBUG=$(PROJECT_ROOT)/output/debug
OUTLIB_DIR_DEBUG=$(PROJECT_ROOT)/output/debug

$(OUTLIB_DIR_DEBUG)/libsme.a:
	make -C sme debug

$(OUTPUT_DIR_DEBUG)/player_pseudo_state: $(OUTLIB_DIR_DEBUG)/libsme.a
	make -C test/player_pseudo_state debug
	
$(OUTPUT_DIR_DEBUG)/player_explicit_entry_exit: $(OUTLIB_DIR_DEBUG)/libsme.a
	make -C test/player_explicit_entry_exit debug

$(OUTPUT_DIR_DEBUG)/player_ortho: $(OUTLIB_DIR_DEBUG)/libsme.a
	make -C test/player_ortho_state debug

$(OUTPUT_DIR_DEBUG)/player_state_timer: $(OUTLIB_DIR_DEBUG)/libsme.a
	make -C test/player_state_timer debug


TARGETS_DEBUG=$(OUTLIB_DIR_DEBUG)/libsme.a \
	      $(OUTPUT_DIR_DEBUG)/player_pseudo_state \
	      $(OUTPUT_DIR_DEBUG)/player_explicit_entry_exit \
	      $(OUTPUT_DIR_DEBUG)/player_ortho \
	      $(OUTPUT_DIR_DEBUG)/player_state_timer
	      
	      

#########################################################
# Release version
#########################################################

OUTPUT_DIR_RELEASE=$(PROJECT_ROOT)/output/release
OUTLIB_DIR_RELEASE=$(PROJECT_ROOT)/output/release

$(OUTLIB_DIR_RELEASE)/libsme.a:
	make -C sme release

$(OUTPUT_DIR_RELEASE)/player_pseudo_state: $(OUTLIB_DIR_RELEASE)/libsme.a
	make -C test/player_pseudo_state release
	
$(OUTPUT_DIR_RELEASE)/player_explicit_entry_exit: $(OUTLIB_DIR_RELEASE)/libsme.a
	make -C test/player_explicit_entry_exit release

$(OUTPUT_DIR_RELEASE)/player_ortho: $(OUTLIB_DIR_RELEASE)/libsme.a
	make -C test/player_ortho_state release

$(OUTPUT_DIR_RELEASE)/player_state_timer: $(OUTLIB_DIR_RELEASE)/libsme.a
	make -C test/player_state_timer release

TARGETS_RELEASE=$(OUTLIB_DIR_DEBUG)/libsme.a \
	      $(OUTPUT_DIR_RELEASE)/player_pseudo_state \
	      $(OUTPUT_DIR_RELEASE)/player_explicit_entry_exit \
	      $(OUTPUT_DIR_RELEASE)/player_ortho \
	      $(OUTPUT_DIR_RELEASE)/player_state_timer


#########################################################
# Misc targets
#########################################################

debug::$(TARGETS_DEBUG) 
release::$(TARGETS_RELEASE) 

clean::
	make -C sme clean
	make -C test/player_pseudo_state clean
	make -C test/player_explicit_entry_exit clean
	make -C test/player_ortho_state clean
	make -C test/player_state_timer clean

