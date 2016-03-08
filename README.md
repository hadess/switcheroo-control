switcheroo-control
==================

D-Bus service to check the availability of dual-GPU

See https://developer.gnome.org/switcheroo-control/1.0/ for
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

Tested on
---------

- MacBook Pro (8,2)

