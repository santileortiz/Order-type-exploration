#!/usr/bin/python3
from mkpy import utility as cfg
from mkpy.utility import *

assert sys.version_info >= (3,2)

DEP_FLAGS = '-lcairo ' \
            '-lX11-xcb ' \
            '-lX11 ' \
            '-lxcb ' \
            '-lxcb-sync ' \
            '-lpthread ' \
            '-lm '

# Put Pango flags separated because some targets don't depend on it.
# NOTE: Not as clean as I would like but Gnome places its headers in weird
# places.
pkg_config_pkgs = ['pango', 'pangocairo']
PKG_CONFIG_L = pers_func ('PKG_CONFIG_L', pkg_config_libs, pkg_config_pkgs)
PKG_CONFIG_I = pers_func ('PKG_CONFIG_I', pkg_config_includes, pkg_config_pkgs)
PANGO_FLAGS = PKG_CONFIG_I + ' ' + PKG_CONFIG_L

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
        'debug': '-Og -g -Wall',
        'profile_debug': '-O2 -g -pg -Wall',
        'release': '-O2 -g -DNDEBUG -DRELEASE_BUILD -Wall'
        }
cli_mode = get_cli_option('-M,--mode', modes.keys())
FLAGS = modes[pers('mode', 'debug', cli_mode)]
ensure_dir ("bin")

def default():
    target = pers ('last_target', 'point_set_viewer')
    call_user_function(target)

def point_set_viewer ():
    ex ('gcc {FLAGS} -o bin/point-set-viewer point_set_viewer.c {PANGO_FLAGS} {DEP_FLAGS}')

def search ():
    ex ('gcc {FLAGS} -o bin/search search.c -lm')

def render_seq ():
    ex ('gcc {FLAGS} -o bin/render_seq render_seq.c -lcairo -lm')

def save_file_to_pdf ():
    ex ('gcc {FLAGS} -o bin/save_file_to_pdf save_file_to_pdf.c {PANGO_FLAGS} -lcairo -lm')

def install ():
    bin_path = pathlib.Path('bin/point-set-viewer')
    if not bin_path.exists():
        print ('No binary, run: ./pymk.py point_set_viewer')
        return

    destdir = get_cli_option('--destdir', has_argument=True)
    installed_files = install_files (installation_info, destdir)
    if destdir == None or destdir == '/':
        for f in installed_files:
            if 'hicolor' in f:
                ex ('gtk-update-icon-cache-3.0 /usr/share/icons/hicolor/')
                break;

cfg.builtin_completions = ['--get_run_deps', '--get_build_deps']
if __name__ == "__main__":
    # Everything above this line will be executed for each TAB press.
    # If --get_completions is set, handle_tab_complete() calls exit().
    handle_tab_complete ()

    pymk_default()

