#!/usr/bin/python3
from mkpy.utility import *
import shutil
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

install_files = {
    'bin/ot_viewer': 'usr/bin/',
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

def default():
    target = pers ('last_target', 'ot_viewer')
    call_user_function(target)

def ot_viewer ():
    os.makedirs ("bin", exist_ok=True)
    ex ('gcc {FLAGS} -o bin/ot_viewer ot_viewer.c {DEP_FLAGS} {PANGO_FLAGS} -lcurl')
    return

def search ():
    os.makedirs ("bin", exist_ok=True)
    ex ('gcc {FLAGS} -o bin/search search.c -lm -lcurl')
    return

def render_seq ():
    os.makedirs ("bin", exist_ok=True)
    ex ('gcc {FLAGS} -o bin/render_seq render_seq.c -lcurl -lcairo -lm')
    return

def save_file_to_pdf ():
    os.makedirs ("bin", exist_ok=True)
    ex ('gcc {FLAGS} -o bin/save_file_to_pdf save_file_to_pdf.c {PANGO_FLAGS} -lcairo -lm')

def install ():
    destdir = get_cli_option('--destdir', has_argument=True)
    if destdir == None:
        destdir = '/'

    for f in install_files.keys():
        dst = destdir + install_files[f]
        if not os.path.exists (dst):
            os.makedirs (dst)
        shutil.copy (f, dst)
        print (install_files[f])

if __name__ == "__main__":
    if get_cli_option ('--get_deps_pkgs'):
        get_deps_pkgs (DEP_FLAGS)
        exit()
    pymk_default()

