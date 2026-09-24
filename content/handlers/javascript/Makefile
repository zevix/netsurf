#
# NetSurf javascript source file inclusion
#
# Included by Makefile
#
# Changed 2026-09-25 for netsurf_upy (GPLv2 section 2(a), a dated notice
# of change).  Three things moved out of the `ifeq` below, because none
# of them is an engine:
#
#   content.c   the accepted-MIME list and the CONTENT_JS handler.  It
#               was listed only in the duktape arm, so a
#               NETSURF_USE_DUKTAPE := NO build registered no script
#               content handler at all.
#   engine.c    the js_* entry points of js.h, and the loader that can
#               take an engine from a shared object at run time.
#   core.c      the callback table a loaded engine is given.
#
# The arm itself still chooses which engine is *built in*; both arms now
# provide ns_builtin_script_engine() rather than the js_* externs.
#

S_JAVASCRIPT_BINDING:=

ifeq ($(NETSURF_USE_DUKTAPE),YES)
# Duktape
include content/handlers/javascript/duktape/Makefile
else
# None
include content/handlers/javascript/none/Makefile
endif

# The engine seam and the content handler are in every build, whichever
# engine the arm above compiled.
S_JAVASCRIPT += content.c engine.c core.c

# Fetcher for javascript scheme is always required
S_JAVASCRIPT += fetcher.c
