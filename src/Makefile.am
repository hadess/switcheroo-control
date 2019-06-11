BUILT_SOURCES =					\
	switcheroo-control-resources.c		\
	switcheroo-control-resources.h

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/switcheroo-control.gresource.xml)
switcheroo-control-resources.c: switcheroo-control.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name switcheroo_control $<
switcheroo-control-resources.h: switcheroo-control.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name switcheroo_control $<

libexec_PROGRAMS = switcheroo-control

switcheroo_control_SOURCES =			\
	switcheroo-control.c			\
	$(BUILT_SOURCES)

switcheroo_control_CPPFLAGS =			\
	$(SWITCHEROO_CONTROL_CFLAGS)		\
	$(WARN_CFLAGS)
switcheroo_control_LDADD = $(SWITCHEROO_CONTROL_LIBS)

EXTRA_DIST =					\
	net.hadess.SwitcherooControl.xml	\
	switcheroo-control.gresource.xml
