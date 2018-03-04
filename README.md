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
  * curl
  * Pango
  * Glib
  
In elementaryOS install them with:

    sudo apt-get install libcairo2-dev libcurl4-openssl-dev libglib2.0-dev libpango1.0-dev libx11-dev libx11-xcb-dev libxcb1-dev libxcb-sync-dev

    
Build with:

    ./pymk point_set_viewer
