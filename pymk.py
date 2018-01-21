#!/usr/bin/python3
from mkpy.utility import *

DEP_FLAGS = '-lcairo ' \
            '-lX11-xcb ' \
            '-lX11 ' \
            '-lxcb ' \
            '-lxcb-sync ' \
            '-lpthread ' \
            '-lm '

# NOTE: This is too much just to depend on Pango, maybe move to HarfBuzz?
PANGO_FLAGS = '-lgobject-2.0 ' \
              '-lpango-1.0 ' \
              '-lpangocairo-1.0 ' \
              '-I/usr/include/pango-1.0 ' \
              '-I/usr/include/cairo ' \
              '-I/usr/include/glib-2.0 ' \
              '-I/usr/lib/x86_64-linux-gnu/glib-2.0/include '

modes = {
        'debug': '-g -Wall',
        'release': '-O3 -DNDEBUG -Wall',
        'profile_release': '-O3 -pg -Wall'
        }
cli_mode = get_cli_option('-M,--mode', modes.keys())
FLAGS = modes[pers('mode', 'debug', cli_mode)]

def default():
    target = pers ('last_target', 'ot_viewer')
    call_user_function(target)

def ot_viewer ():
    ex ('gcc {FLAGS} -o bin/ot_viewer ot_viewer.c {DEP_FLAGS} {PANGO_FLAGS} -lcurl')
    return

def search ():
    ex ('gcc {FLAGS} -o bin/search search.c -lm -lcurl')
    return

def render_seq ():
    ex ('gcc {FLAGS} -o bin/render_seq render_seq.c -lcurl -lcairo -lm')
    return

def save_file_to_pdf ():
    ex ('gcc {FLAGS} -o bin/save_file_to_pdf save_file_to_pdf.c {PANGO_FLAGS} -lcairo -lm')

if __name__ == "__main__":
    if get_cli_option ('--get_deps_pkgs'):
        get_deps_pkgs (DEP_FLAGS)
        exit()
    pymk_default()

