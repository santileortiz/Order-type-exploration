import sys, subprocess, os

import importlib.util, inspect, pathlib, filecmp
def get_user_functions():
    """
    Executes pymk.py with __name__=="pymk" and returns a list with tuples of
    its functions [(f_name, f), ...].

    NOTE: Functions imported like 'from <module> import <function>' are included too.
    """
    spec = importlib.util.spec_from_file_location('pymk', 'pymk.py')
    pymk_user = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(pymk_user)
    return inspect.getmembers(pymk_user, inspect.isfunction).copy()

def get_user_function(name):
    for f_name, f in get_user_functions():
        if f_name == name:
            return f
    return None

def get_completions ():
    keys = globals().copy().keys()
    for m,v in get_user_functions():
        if  m in keys:
            # Ignore functions in this file when user
            # imports everything from here.
            continue
        print (str(m))

def check_completions ():
    comp_path = pathlib.Path('/usr/share/bash-completion/completions/pymk.py')
    if not comp_path.exists():
        warn ('Tab completions not installed:')
        print ('Use "sudo ./pymk.py --install_completions" to install them\n')
        return
    if comp_path.is_file():
        if not filecmp.cmp ('mkpy/pymk.py', str(comp_path)):
            err ('Tab completions outdated:')
            print ('Update with "sudo ./pymk.py --install_completions"\n')
    else:
        err ('Something funky is going on.')

def install_completions ():
    ex ("cp mkpy/pymk.py /usr/share/bash-completion/completions/")
    return

def err (string):
    print ('\033[1m\033[91m{}\033[0m'.format(string))

def ok (string):
    print ('\033[1m\033[92m{}\033[0m'.format(string))

def get_user_str_vars ():
    """
    Executes pymk.py with __name__=="pymk" and returns a dictionary
    with its string variables.
    """
    spec = importlib.util.spec_from_file_location('pymk', 'pymk.py')
    pymk_user = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(pymk_user)
    var_list = inspect.getmembers(pymk_user)
    var_dict = {}
    for v_name, v in var_list:
        if type(v) == type(""):
            var_dict[v_name] = v
    return var_dict

def ex (cmd, no_stdout=False, echo=True):
    resolved_cmd = cmd.format(**get_user_str_vars())
    if echo: print (resolved_cmd)
    redirect = open(os.devnull, 'wb') if no_stdout else None
    return subprocess.call(resolved_cmd, shell=True, stdout=redirect)

def pymk_default ():
    if len(sys.argv) == 1:
        check_completions ()
        f = get_user_function("default")
        f() if f else print ('No default function.')
        return

    if (sys.argv[1] == '--get_completions'):
        get_completions()
        return
    elif sys.argv[1] == '--install_completions':
        install_completions ()
        return
    else:
        check_completions ()
        f = get_user_function (sys.argv[1])
        if f != None:
            f()
        else:
            print ('No function "' + sys.argv[1] + '".')
