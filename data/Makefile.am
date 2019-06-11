systemdservicedir = $(systemdsystemunitdir)
systemdservice_in_files = switcheroo-control.service.in
systemdservice_DATA = switcheroo-control.service
switcheroo-control.service: switcheroo-control.service.in Makefile
	$(AM_V_GEN) $(SED) -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

dbusconfdir = $(sysconfdir)/dbus-1/system.d
dbusconf_DATA = net.hadess.SwitcherooControl.conf

CLEANFILES = switcheroo-control.service
EXTRA_DIST = $(dbusconf_DATA) $(systemdservice_in_files)
