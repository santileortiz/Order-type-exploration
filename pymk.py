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

modes = {
        'debug': '-g -Wall',
        'release': '-O3 -DNDEBUG -Wall',
        'profile_release': '-O3 -pg -Wall'
        }
cli_mode = get_cli_option('-M,--mode', modes.keys())
FLAGS = modes[pers('mode', 'debug', cli_mode)]

def default():
    target = pers ('last_target', 'database_expl')
    call_user_function(target)

def database_expl ():
    ex ('gcc {FLAGS} database_expl.c -o bin/database_expl {DEP_FLAGS} -lcurl')
    return

def search ():
    ex ('gcc {FLAGS} -o bin/search search.c -lm -lcurl')
    return

def render_seq ():
    ex ('gcc {FLAGS} -o bin/render_seq render_seq.c -lcurl -lcairo -lm')
    return

def save_file_to_pdf ():
    ex ('gcc {FLAGS} -o bin/save_file_to_pdf save_file_to_pdf.c -lcairo -lm')

if __name__ == "__main__":
    if get_cli_option ('--get_deps_pkgs'):
        get_deps_pkgs (DEP_FLAGS)
        exit()
    pymk_default()

