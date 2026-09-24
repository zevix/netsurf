#
# NetSurf javascript source file inclusion
#
# Included by javascript/Makefile
#
# Changed 2026-09-25 for netsurf_upy (GPLv2 section 2(a), a dated notice
# of change).  `content.c` left this file -- it is the accepted-MIME
# list, not part of the engine, and listing it here made a
# `NETSURF_USE_DUKTAPE := NO` build one with no CONTENT_JS handler at
# all.  And `NETSURF_DUKTAPE_PLUGIN := YES` builds the same engine a
# second time as `ns_engine_duktape.so`, a run-time-loadable
# `struct ns_script_engine_v1` (see `javascript/engine.h`).
#

content/handlers/javascript/duktape/dukky.c: \
	$(OBJROOT)/duktape/binding.h \
	$(OBJROOT)/duktape/generics.js.inc \
	$(OBJROOT)/duktape/polyfill.js.inc

BINDINGS := $(wildcard content/handlers/javascript/duktape/*.bnd)

# Generator for the C include representing the generics.js
$(OBJROOT)/duktape/generics.js.inc: content/handlers/javascript/duktape/generics.js $(TOOLROOT)/xxd
	$(Q)$(MKDIR) -p $(OBJROOT)/duktape
	$(VQ)echo "     XXD: $<"
	$(Q)$(TOOLROOT)/xxd -i $< $@.tmp
	$(Q)sed -e 's/content_handlers_javascript_duktape_generics_js/generics_js/' $@.tmp > $@

# Generator for the C include representing the polyfill.js
$(OBJROOT)/duktape/polyfill.js.inc: content/handlers/javascript/duktape/polyfill.js $(TOOLROOT)/xxd
	$(Q)$(MKDIR) -p $(OBJROOT)/duktape
	$(VQ)echo "     XXD: $<"
	$(Q)$(TOOLROOT)/xxd -i $< $@.tmp
	$(Q)sed -e 's/content_handlers_javascript_duktape_polyfill_js/polyfill_js/' $@.tmp > $@

# ensure genbind generates debugging files
GBFLAGS+=-D

$(OBJROOT)/duktape/binding.h $(OBJROOT)/duktape/Makefile: content/handlers/javascript/duktape/netsurf.bnd $(BINDINGS)
	$(Q)$(MKDIR) -p $(OBJROOT)/duktape
	$(VQ)echo " GENBIND: $<"
	$(Q)nsgenbind $(GBFLAGS) -I content/handlers/javascript/WebIDL $< $(OBJROOT)/duktape

# create unimplemented report for doxygen
docs/UnimplementedJavascript.md: content/handlers/javascript/duktape/netsurf.bnd $(BINDINGS)
	$(Q)$(MKDIR) -p $(OBJROOT)/duktape
	$(VQ)echo "Unimplemented javascript bindings {#unimplemented}" > $@
	$(VQ)echo "=================================" >> $@
	$(VQ)echo "" >> $@
	$(VQ)echo "This is a list of all the binding methods, getters and setters without an implementation in a binding." >> $@
	$(VQ)echo "" >> $@
	$(VQ)echo "Type | Unimplemented binding" >> $@
	$(VQ)echo "---- | ---------------------" >> $@
	$(VQ)echo " GENBIND: $<"
	$(Q)nsgenbind $(GBFLAGS) -Wunimplemented -I content/handlers/javascript/WebIDL $< $(OBJROOT)/duktape 2>&1 >/dev/null | awk 'BEGIN{count=0} /.*Unimplemented.*/{count++; out=$$4" |"; for(i=5;i<=NF;i++){out=out" "$$i}; print out} END{print "\n",count,"unimplemented bindings"}' >> $@
	$(VQ)echo "" >> $@

ifeq ($(filter $(MAKECMDGOALS),clean test coverage),)
-include $(OBJROOT)/duktape/Makefile
endif

S_JAVASCRIPT_BINDING:=$(addprefix $(OBJROOT)/duktape/,$(NSGENBIND_SOURCES))

$(S_JAVASCRIPT_BINDING): $(BINDINGS)

S_JAVASCRIPT += duktape/dukky.c duktape/duktape.c

CFLAGS += -DDUK_OPT_HAVE_CUSTOM_H


# ---------------------------------------------------------------------
# The same engine again, as a loadable shared object (netsurf_upy G4)
# ---------------------------------------------------------------------
#
# Not a second engine and not a fork of one: the same `dukky.c`, the
# same `duktape.c` and the same generated bindings, compiled `-fPIC`
# with NS_ENGINE_PLUGIN defined.  That swaps `ns_builtin_script_engine()`
# for the exported `ns_script_engine_v1_get()` and force-includes
# `javascript/core.h`, whose `#define`s send every browser symbol the
# engine calls through the callback table it is handed.
#
# `-Wl,--no-undefined` is the guard that makes `javascript/core.def`
# trustworthy: a symbol the engine needs and the table does not carry is
# a link error naming it, here, rather than a `dlopen` failure on the
# machine.  The finished object imports the C library and nothing else.
#
# Resolving the engine's host symbols from the executable instead -- by
# leaving them undefined and linking the browser `-rdynamic` -- was
# measured on 2026-09-24 and is not what this does; see
# `javascript/core.h`.

ifeq ($(NETSURF_DUKTAPE_PLUGIN),YES)

NSENGINE_ROOT := $(OBJROOT)/engine
NSENGINE_SO := $(OBJROOT)/ns_engine_duktape.so

#: `javascript/core.h` includes this; it is `core.def` with each name
#: turned into the `#define` that sends it through the table.  Two `sed`
#: expressions, so the list itself is written exactly once.
NSENGINE_CORE_H := $(NSENGINE_ROOT)/javascript/core_plugin.h

$(NSENGINE_CORE_H): content/handlers/javascript/core.def
	$(Q)$(MKDIR) -p $(NSENGINE_ROOT)/javascript
	$(VQ)echo "  GENDEF: $<"
	$(Q)echo "/* Generated from $< -- do not edit. */" > $@.tmp
	$(Q)sed -n \
	    -e 's|^NS_CORE_FN(\(.*\))$$|#define \1 (ns_script_core->\1)|p' \
	    -e 's|^NS_CORE_DATA(\(.*\))$$|#define \1 (*ns_script_core->\1)|p' \
	    $< >> $@.tmp
	$(Q)mv $@.tmp $@

NSENGINE_CFLAGS := -fPIC -DNS_ENGINE_PLUGIN \
	-I$(NSENGINE_ROOT) -include javascript/core.h

#: The engine's own two sources, and every source nsgenbind generated.
NSENGINE_OBJS := $(NSENGINE_ROOT)/dukky.o $(NSENGINE_ROOT)/duktape.o \
	$(NSENGINE_ROOT)/core_thunks.o \
	$(addprefix $(NSENGINE_ROOT)/,$(notdir $(NSGENBIND_SOURCES:.c=.o)))

$(NSENGINE_ROOT)/created: $(OBJROOT)/created
	$(Q)$(MKDIR) -p $(NSENGINE_ROOT)
	$(Q)$(TOUCH) $(NSENGINE_ROOT)/created

# `duktape.c` is the upstream VM.  It calls nothing of NetSurf's --
# `dukky_check_timeout` is its only import from us -- so it does not
# take the force-included header, and keeping it out means the one file
# in the tree with its own feature-test macros is compiled exactly as
# the browser compiles it.
$(NSENGINE_ROOT)/duktape.o: content/handlers/javascript/duktape/duktape.c \
		$(NSENGINE_ROOT)/created
	$(VQ)echo " PLUGIN: $<"
	$(Q)$(CC) $(COMMON_WARNFLAGS) $(CWARNFLAGS) $(IFLAGS) $(CFLAGS) \
		  $(CFLAGS_ENV) -fPIC -o $@ -c $<

$(NSENGINE_ROOT)/dukky.o: content/handlers/javascript/duktape/dukky.c \
		$(NSENGINE_CORE_H) $(NSENGINE_ROOT)/created \
		$(OBJROOT)/duktape/binding.h
	$(VQ)echo " PLUGIN: $<"
	$(Q)$(CC) $(COMMON_WARNFLAGS) $(CWARNFLAGS) $(IFLAGS) $(CFLAGS) \
		  $(CFLAGS_ENV) $(NSENGINE_CFLAGS) -o $@ -c $<

$(NSENGINE_ROOT)/core_thunks.o: content/handlers/javascript/core_thunks.c \
		$(NSENGINE_CORE_H) $(NSENGINE_ROOT)/created
	$(VQ)echo " PLUGIN: $<"
	$(Q)$(CC) $(COMMON_WARNFLAGS) $(CWARNFLAGS) $(IFLAGS) $(CFLAGS) \
		  $(CFLAGS_ENV) $(NSENGINE_CFLAGS) -o $@ -c $<

$(NSENGINE_ROOT)/%.o: $(OBJROOT)/duktape/%.c $(NSENGINE_CORE_H) \
		$(NSENGINE_ROOT)/created
	$(VQ)echo " PLUGIN: $<"
	$(Q)$(CC) $(COMMON_WARNFLAGS) $(CWARNFLAGS) $(IFLAGS) $(CFLAGS) \
		  $(CFLAGS_ENV) $(NSENGINE_CFLAGS) -o $@ -c $<

NSENGINE_MAP := content/handlers/javascript/engine.map

$(NSENGINE_SO): $(NSENGINE_OBJS) $(NSENGINE_MAP)
	$(VQ)echo " PLUGIN: $@"
	$(Q)$(CC) -shared -o $@ $(NSENGINE_OBJS) -Wl,--no-undefined \
		  -Wl,--version-script=$(NSENGINE_MAP) -lm

POSTEXES += $(NSENGINE_SO)

#: A plugin that loads, answers and declares the wrong ABI, so that the
#: refusal path has something to refuse.  It shares no header with the
#: browser on purpose -- it is the ABI contract seen from outside.
NSENGINE_BAD_SO := $(OBJROOT)/ns_engine_abi_mismatch.so

$(NSENGINE_BAD_SO): content/handlers/javascript/engine_abi_mismatch.c \
		$(OBJROOT)/created
	$(VQ)echo " PLUGIN: $@"
	$(Q)$(CC) $(COMMON_WARNFLAGS) $(CWARNFLAGS) -fPIC -shared -o $@ $< \
		  -Wl,--no-undefined

POSTEXES += $(NSENGINE_BAD_SO)

clean-engine-plugin:
	$(VQ)echo "   CLEAN: $(NSENGINE_SO)"
	$(Q)$(RM) -r $(NSENGINE_ROOT) $(NSENGINE_SO) $(NSENGINE_BAD_SO)
CLEANS += clean-engine-plugin

endif
