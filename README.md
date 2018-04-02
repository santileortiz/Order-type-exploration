Point Set Viewer
=================

Graphical application to view point sets representing order types.

![screenshot](img/screenshot.png)

Compilation
-----------

Dependencies:
  * Cairo
  * X11 (XLib and xcb)
  * xcb-sync
  * Pango
  
In elementaryOS/Ubuntu install them with:

    sudo apt-get install libcairo2-dev libpango1.0-dev libx11-xcb-dev libxcb-sync-dev

In Fedora install them with:

    sudo dnf install cairo-devel gcc pango-devel

Build with:

    ./pymk point_set_viewer
