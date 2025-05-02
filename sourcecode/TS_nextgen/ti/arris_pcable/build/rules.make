#  Copyright 2014, ARRIS Group, Inc., All rights reserved                                        

# Include the Unified Makefile Framework Rules Makefile
include $(TARGET_HOME)/ti/system/make/rules.make

# Pre-process LEX and YACC
###############################################################################
BISON = @bison -d --debug --verbose --report=state
FLEX = @flex -d
LEXSUBST = $(<:.l=.lex.c)
YACCSUBST = $(<:.y=)
START_COLOR = -en "\\033[1;34m"
END_COLOR = "\\033[0m\n"
ECHO_LEX_PROCESSING = @echo $(START_COLOR) ' Pre-processing LEX file $(notdir $<)' $(END_COLOR)
ECHO_YACC_PROCESSING = @echo $(START_COLOR) ' Pre-processing YACC file $(notdir $<)' $(END_COLOR)

# mark the output of FLEX and BISON as secondary targets, so make won't delete them
.SECONDARY : callp_sdp.lex.c callp_sdp.tab.c                        \
             callp_sgcp.lex.c callp_sgcp.tab.c                      \
             sip_broadsoft_info.lex.c sip_broadsoft_info.tab.c      \
             sip_mwi.lex.c sip_mwi.tab.c                            \
             sip_n2p_call_forward.lex.c sip_n2p_call_forward.tab.c  \

%sgcp : %sgcp.tab.c %sgcp.lex.c
%sdp : %sdp.tab.c %sdp.lex.c 

# NCS pre-processing
%sgcp.lex.c: %sgcp.l
	$(ECHO_LEX_PROCESSING)
	$(FLEX) -o$(<:.l=.lex.c) -Pcallp_yy $<

%sdp.lex.c: %sdp.l
	$(ECHO_LEX_PROCESSING)
	$(FLEX) -o$(<:.l=.lex.c) -Pcallp_sdp_yy $<

%sgcp.tab.c: %sgcp.y
	$(ECHO_YACC_PROCESSING)
	$(BISON) -b $(<:.y=) -p callp_yy $<

%sdp.tab.c: %sdp.y
	$(ECHO_YACC_PROCESSING)
	$(BISON) -b $(<:.y=) -p callp_sdp_yy $<

%info : %info.tab.c %info.lex.c
%mwi : %mwi.tab.c %mwi.lex.c
%forward : %forward.tab.c %forward.lex.c

# SIP pre-processing
%info.lex.c : %info.l
	$(ECHO_LEX_PROCESSING)
	$(FLEX) -o$(LEXSUBST) -Psip_broadsoft_info_yy $<
%mwi.lex.c : %mwi.l
	$(ECHO_LEX_PROCESSING)
	$(FLEX) -o$(LEXSUBST) -Psip_mwi_yy $<
%forward.lex.c : %forward.l
	$(ECHO_LEX_PROCESSING)
	$(FLEX) -o$(LEXSUBST) -Psip_n2p_call_forward_yy $<
%info.tab.c : %info.y
	$(ECHO_YACC_PROCESSING)
	$(BISON) -b $(YACCSUBST) -p sip_broadsoft_info_yy $<
%mwi.tab.c : %mwi.y
	$(ECHO_YACC_PROCESSING)
	$(BISON) -b $(YACCSUBST) -p sip_mwi_yy $<
%forward.tab.c : %forward.y
	$(ECHO_YACC_PROCESSING)
	$(BISON) -b $(YACCSUBST) -p sip_n2p_call_forward_yy $<
###############################################################################

