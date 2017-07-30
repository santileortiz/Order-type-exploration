#!/usr/bin/python3
from mkpy.utility import *

# NOTE: Last ones are only required for Pango, maybe move to HarfBuzz?
DEP_FLAGS = '-lcairo ' \
            '-lX11-xcb ' \
            '-lX11 ' \
            '-lxcb ' \
            '-lxcb-sync ' \
            '-lm ' \
            \
            '-lpango-1.0 ' \
            '-lpangocairo-1.0 ' \
            '-I/usr/include/pango-1.0 ' \
            '-I/usr/include/cairo ' \
            '-I/usr/include/glib-2.0 ' \
            '-I/usr/lib/x86_64-linux-gnu/glib-2.0/include '

FLAGS = '-g -Wall' #Debug
#FLAGS = '-O3 -DNDEBUG -Wall' #Release
#FLAGS = '-O3 -pg -Wall #Profile' release

def database_expl ():
    ex ('gcc {FLAGS} database_expl.c -o bin/database_expl {DEP_FLAGS}')
    return

def search ():
    ex ('gcc {FLAGS} -o bin/search search.c -lm')
    return

def save_file_to_pdf ():
    ex ('gcc {FLAGS} -o bin/save_file_to_pdf save_file_to_pdf.c -lcairo -lm')

if __name__ == "__main__":
    pymk_default()

