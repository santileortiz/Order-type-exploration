Order Type Viewer
=================

Graphical application to view point sets representing order types.

Compilation
-----------

Dependencies:
  * Cairo
  * X11 (XLib and xcb)
  * xcb-sync
  * Pango
  
In elementaryOS install them with:

    sudo apt-get install libcairo2-dev libx11-dev libxcb1-dev libxcb-sync-dev libpango1.0-dev
    
Build with:

    ./pymk ot_viewer
