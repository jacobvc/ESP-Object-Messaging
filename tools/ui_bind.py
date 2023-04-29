import os
import re
import sys
import PySimpleGUI as sg
import json
import getopt

helptext = '''
Collect symbols from include_file lines starting with 'extern lv_obj_t'.
Build a UI with Produce and Consume checkboxes for each symbol starting with
a recognized three character prefix (and indicating any that do not start
with a recognized sequence).

'Save' generates <binding_name>.cpp with Produce / Comsume code for the checked
items, and <binding_name>.json to save / restore the settings.

'''
events = [
    "LV_EVENT_PRESSED",             # The object has been pressed
    "LV_EVENT_PRESSING",            # The object is being pressed (called continuously while pressing)
    "LV_EVENT_PRESS_LOST",          # User is still pressing but slid cursor/finger off of the object 
    "LV_EVENT_SHORT_CLICKED",       # User pressed object for a short period of time, then released it. Not called if dragged. 
    "LV_EVENT_LONG_PRESSED",        # Object has been pressed for at least `LV_INDEV_LONG_PRESS_TIME`.  Not called if dragged.
    "LV_EVENT_LONG_PRESSED_REPEAT", # Called after `LV_INDEV_LONG_PRESS_TIME` in every
                                  #   `LV_INDEV_LONG_PRESS_REP_TIME` ms.  Not called if dragged.
    "LV_EVENT_CLICKED",             # Called on release if not dragged (regardless to long press)
    "LV_EVENT_RELEASED",            # Called in every cases when the object has been released
    "LV_EVENT_DRAG_BEGIN",
    "LV_EVENT_DRAG_END",
    "LV_EVENT_DRAG_THROW_BEGIN",
    "LV_EVENT_GESTURE",           # The object has been gesture
    "LV_EVENT_KEY",
    "LV_EVENT_FOCUSED",
    "LV_EVENT_DEFOCUSED",
    "LV_EVENT_LEAVE",
    "LV_EVENT_VALUE_CHANGED",      # The object's value has changed (i.e. slider moved) 
    "LV_EVENT_INSERT",
    "LV_EVENT_REFRESH",
    "LV_EVENT_APPLY",  # "Ok", "Apply" or similar specific button has clicked
    "LV_EVENT_CANCEL", # "Close", "Cancel" or similar specific button has clicked
    "LV_EVENT_DELETE", # Object is being deleted 
]

# initialized data
typemaps = dict();
typemaps['lbl'] = "LABEL_ID";
typemaps['txa'] = "TEXTAREA_ID";
typemaps['cmb'] = "COMBO_ID"
typemaps['led'] = "LED_ID"
typemaps['gau'] = "GAUGE_ID"
typemaps['btn'] = "BUTTON_ID"
typemaps['sld'] = "SLIDER_ID"
typemaps['swt'] = "SWITCH_ID"

declare_re = r"extern lv_obj_t.\*\s*(\w+)";
NAME_SIZE = 16

# Configuration variables
working_dir = './'
include_file = "ui.h";
binding_file = "ui_binding";
constant_prefix = "ui_";
verbose = False

# dynamic variables
variables = []
layout = []     
notes = ''

# Misc initialization
sg.set_options(suppress_raise_key_errors=False, 
               suppress_error_popups=True, 
               suppress_key_guessing=True)

def main():
    global notes
    if verbose: show_config()

    process_include()

    layout.append([sg.Multiline(notes, size=(None, 5), k='notes')])
    layout.append([sg.Button('Save', button_color=('white', 'black')),
            sg.Button('Exit', button_color=('white', 'black'))])
    window = sg.Window('Produce / Consume Configuration', layout, font="_ 14", finalize=True)
    
    # AFTER the window is finalized
    load_config(window)

    window['notes'].update(notes)

    while True:
        event, values = window.read()
        if event == sg.WINDOW_CLOSED or event == 'Exit':
            break

        if event == 'Save':
            save(values)

    window.close();

#
# Action functions
#

def process_include():
    if verbose: print('Processing include file: ' + working_dir + include_file)
    try:
        f = open(working_dir + include_file);
    except Exception as e:
        print(e)
        exit()
    contents = f.readlines();
    f.close();

    for line in contents:
        global notes
        match = re.search(declare_re, line);
        if match:
            variable = match.group(1);
            symbol_name = get_symbol_name(variable);
            if symbol_name[0:3] in typemaps:
                add_variable(variable)
            else:
                notes += "'" + symbol_name + "' produce/consume mapping not supported\n";

def load_config(window):
    global notes
    if verbose: print('Loading config file: ' + working_dir + binding_file + ".json")
    try:
        f = open(working_dir + binding_file + ".json");
        found = json.load(f);
        for key, value in found['values'].items():
            try:
                if verbose: print('  Loading: ' + key)
                if len(key) > 4:
                    window[key].update(value)
            except Exception as e: 
                print("EX 1:" + str(e))
                notes += key[0:-4] + " Not in current code. Skipping\n"
                #print(notes)
        f.close();
    except Exception as e:
        #print(e)
        print('Warning "' + binding_file + '.json" not found')

def save(values):
    jdata = {}
    jdata['variables'] = variables
    jdata['values'] = values
    if verbose: print('Saving config file: ' + working_dir + binding_file + ".json")
    f = open(working_dir + binding_file + ".json", "w");
    f.write(json.dumps(jdata, indent=2))
    f.close();

    if verbose: print('Saving output file: ' + working_dir + binding_file + ".cpp")
    f = open(working_dir + binding_file + ".cpp", "w");
    f.write('#include "LvglHost.h"\n');
    f.write('#include "' + include_file + '"\n');
    f.write('#include "binding.h"\n\n');
    f.write("void ui_binding_init(LvglHost& host)\n{\n");
    for variable in variables:
        write_function(f, variable, values[variable + '_pro'], values[variable + '_con'], values[variable + '_evt'])
    f.write("}\n");
    f.close();
#
# Utility functions
#

def sg_name(name):
    dots = NAME_SIZE-len(name)-2
    return sg.Text(name + ' ' + ' '*dots, size=(NAME_SIZE,1), justification='l',pad=(0,0))

def get_symbol_name(variable):
    return variable.replace(constant_prefix, "");

def add_variable(variable):
    symbol_name = get_symbol_name(variable);
    if verbose: print('  Adding ' + variable + ' as ' + symbol_name)
    variables.append(variable);
    choices = [sg_name(symbol_name), sg.Checkbox('Produce', k=variable + '_pro'), 
        sg.Checkbox('Consume', k=variable + '_con'),
        sg.Combo(k=variable + '_evt', default_value="LV_EVENT_CLICKED", values=events, font="_ 10")]
    layout.append(choices)

def write_function(f, variable, produce, consume, event):
    if verbose: 
        print('  Writing ' + variable, end=' ')
        if produce: print('Produce', end=' ')
        if consume: print('Consume', end=' ')
        print()
    symbol_name = get_symbol_name(variable);
    body = '("' + symbol_name[3:].lower() + '", ' + variable + ", " + typemaps[symbol_name[0:3]] + ", (1 << " + event + "));\n";
    if produce: 
        f.write('    host.addProducer' + body);
    elif verbose:
        f.write('    // addProducer' + body);
    if consume: 
        f.write('    host.addConsumer' + body);
    elif verbose:
        f.write('    // addConsumer' + body);

def show_config():
    print('Settings')
    print('  working_dir: ' + working_dir)
    print('  include_file: ' + include_file + ' (' + working_dir + include_file + ')')
    print('  binding_file: ' + binding_file + ' (' + working_dir + binding_file + ')')
    print('    Settings: ' + working_dir + binding_file + '.json)')
    print('    Configuration: ' + working_dir + binding_file + '.cpp)')
    print('  Constant prefix: ' + constant_prefix)

def help():
    print(helptext)
    usage()

def usage():
    print(
'''Usage: ui_bind.py [-w <working_dir>][-i <include_file>][-b <binding_name>][-p <prefix>][-v] [-h]'''
    )

# process command line arguments
arglist = sys.argv[1:]

# Options
options = "w:i:b:p:vh"
# Long options doesn't seem to work ... avoid for now
long_options = [] # ["Help", "working_dir=", "include=", "binding_name=", "prefix=", "verbose", help]

try:
    # Parsing argument
    args, values = getopt.getopt(arglist, options, long_options)

    # checking each argument
    for arg, argv in args:

        if arg in ("-h", "--help"):
            help()
            exit();
        if arg in ("-v", "--verbose"):
            verbose = True;
        elif arg in ("-w", "--working_dir"):
            working_dir = argv;
            if not working_dir.endswith('/') and not working_dir.endswith('\\'):
                working_dir += '/'
            print ("Working directory: " + working_dir)
        elif arg in ("-i", "--include"):
            include_file = argv;
            print ("Include file: " + include_file)
        elif arg in ("-b", "--binding_name"):
            binding_file = argv;
            print ("Binding file: " + binding_file)
        elif arg in ("-p", "--prefix"):
            constant_prefix = argv;
            print ("Constant prefix: " + constant_prefix)

except getopt.error as err:
    # output error, and return with an error code
    print (str(err))
    exit()

main()