#!/usr/bin/python3
from mkpy.utility import *
assert sys.version_info >= (3,2)

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

installation_info = {
    'bin/point-set-viewer': 'usr/bin/',
    'data/point-set-viewer.desktop': 'usr/share/applications/',
    'data/icons/128/point-set-viewer.svg': 'usr/share/icons/hicolor/128x128/apps/',
    'data/icons/64/point-set-viewer.svg': 'usr/share/icons/hicolor/64x64/apps/',
    'data/icons/48/point-set-viewer.svg': 'usr/share/icons/hicolor/48x48/apps/',
    'data/icons/32/point-set-viewer.svg': 'usr/share/icons/hicolor/32x32/apps/',
    'data/icons/24/point-set-viewer.svg': 'usr/share/icons/hicolor/24x24/apps/',
    'data/icons/16/point-set-viewer.svg': 'usr/share/icons/hicolor/16x16/apps/',
    }

modes = {
        'debug': '-g -Wall',
        'release': '-O3 -DNDEBUG -Wall',
        'profile_release': '-O3 -pg -Wall'
        }
cli_mode = get_cli_option('-M,--mode', modes.keys())
FLAGS = modes[pers('mode', 'debug', cli_mode)]
ensure_dir ("bin")

def default():
    target = pers ('last_target', 'point-set-viewer')
    call_user_function(target)

def point_set_viewer ():
    ex ('gcc {FLAGS} -o bin/point-set-viewer point_set_viewer.c {DEP_FLAGS} {PANGO_FLAGS} -lcurl')

def search ():
    ex ('gcc {FLAGS} -o bin/search search.c -lm -lcurl')

def render_seq ():
    ex ('gcc {FLAGS} -o bin/render_seq render_seq.c -lcurl -lcairo -lm')

def save_file_to_pdf ():
    ex ('gcc {FLAGS} -o bin/save_file_to_pdf save_file_to_pdf.c {PANGO_FLAGS} -lcairo -lm')

def install ():
    bin_path = pathlib.Path('bin/point-set-viewer')
    if not bin_path.exists():
        print ('No binary, run: ./pymk.py point_set_viewer')
        return

    destdir = get_cli_option('--destdir', has_argument=True)
    install_files (installation_info, destdir)
    if destdir == None or destdir == '/':
        ex ('gtk-update-icon-cache-3.0 /usr/share/icons/hicolor/')

if __name__ == "__main__":
    if get_cli_option ('--get_deps_pkgs'):
        get_target_dep_pkgs ()
        exit ()
    pymk_default()

