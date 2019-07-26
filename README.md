switcheroo-control
==================

D-Bus service to check the availability of dual-GPU

See https://developer.gnome.org/switcheroo-control/ for
developer information.

Installation
------------
```
./configure --prefix=/usr --sysconfdir=/etc
make
make install
```
It requires libgudev and systemd.

```
gdbus introspect --system --dest net.hadess.SwitcherooControl --object-path /net/hadess/SwitcherooControl
```

If that doesn't work, please file an issue, make sure any running switcheroo-control
has been stopped:
`systemctl stop switcheroo-control.service`
and attach the output of:
`G_MESSAGES_DEBUG=all /usr/sbin/switcheroo-control`
running as ```root```.

Disabling automatic switch to integrated GPU
--------------------------------------------

By default, on startup and whatever the BIOS settings (which might or
might not be available, depending on the system), we will try to force the
integrated GPU to be used so that power savings are made by default,
and the discrete GPU is only used for select applications.

If this causes problems, this behaviour can be disabled by passing
`xdg.force_integrated=0` as a kernel command-line options in the
bootloader.

Don't forget to file a bug against your distribution to get the kernel
or graphics drivers fixed, depending on the exact problem at hand.

Tested on
---------

- MacBook Pro (8,2)
- Thinkpad T430s
