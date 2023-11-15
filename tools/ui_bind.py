import os
import re
import sys
import PySimpleGUI as sg
import json
import getopt
from datetime import date

version = "2"

helptext = 'UI Bind - Version ' + version + '''

Collect symbols from include_file lines starting with 'extern lv_obj_t *'.
Build a UI with Produce, Consume, and Group checkboxes each symbol starting
with a suported three character prefix (and reporting any that do not start
with a supported sequence), OR for all symbols (-a option)

'Save' 
  generates <binding_name>.cpp with 
      Produce/Consume code in LvglBindingInit(LvglHost& host)
    and 
      Group code in LvglGroupInit(lv_group_t* group)
    for the checked items, 
  and generates <binding_name>.json to save / restore the settings.
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
    #"LV_EVENT_DRAG_BEGIN",
    #"LV_EVENT_DRAG_END",
    #"LV_EVENT_DRAG_THROW_BEGIN",
    #"LV_EVENT_GESTURE",           # The object has been gesture
    "LV_EVENT_KEY",
    "LV_EVENT_FOCUSED",
    "LV_EVENT_DEFOCUSED",
    "LV_EVENT_LEAVE",
    "LV_EVENT_VALUE_CHANGED",      # The object's value has changed (i.e. slider moved) 
    #"LV_EVENT_INSERT",
    "LV_EVENT_REFRESH",
    #"LV_EVENT_APPLY",  # "Ok", "Apply" or similar specific button has clicked
    #"LV_EVENT_CANCEL", # "Close", "Cancel" or similar specific button has clicked
    #"LV_EVENT_DELETE", # Object is being deleted 
]

# initialized data

# A dictionary of name prefixes and associated 
# control type / default event. If you add a new control type
# it must be added to the ControlType enum in LcvglBinding.h
typemaps = dict()
typemaps['scr'] = ["SCREEN_CT", "LV_EVENT_CLICKED"]
typemaps['arc'] = ["ARC_CT", "LV_EVENT_CLICKED"]
typemaps['btn'] = ["BUTTON_CT", "LV_EVENT_CLICKED"]
typemaps['img'] = ["IMAGE_CT", "LV_EVENT_CLICKED"]
typemaps['lbl'] = ["LABEL_CT", "LV_EVENT_VALUE_CHANGED"]
typemaps['pnl'] = ["PANEL_CT", "LV_EVENT_CLICKED"]
typemaps['txa'] = ["TEXTAREA_CT", "LV_EVENT_VALUE_CHANGED"]
typemaps['cal'] = ["CALENDAR_CT", "LV_EVENT_VALUE_CHANGED"]
typemaps['chk'] = ["CHECKBOX_CT", "LV_EVENT_VALUE_CHANGED"]
typemaps['clr'] = ["COLORWHEEL_CT", "LV_EVENT_VALUE_CHANGED"]
typemaps['cmb'] = ["DROPDOWN_CT", "LV_EVENT_VALUE_CHANGED"]
typemaps['ibt'] = ["IMGBUTTON_CT", "LV_EVENT_CLICKED"]
typemaps['kbd'] = ["KEYBOARD_CT", "LV_EVENT_KEY"]
typemaps['rlr'] = ["ROLLER_CT", "LV_EVENT_VALUE_CHANGED"]
typemaps['sld'] = ["SLIDER_CT", "LV_EVENT_VALUE_CHANGED"]
typemaps['swt'] = ["SWITCH_CT", "LV_EVENT_VALUE_CHANGED"]
#typemaps['led'] = ["LED_CT", "LV_EVENT_VALUE_CHANGED"]
#typemaps['gau'] = ["GAUGE_CT", "LV_EVENT_VALUE_CHANGED"]

# Derive a list of the mapped types
types = list(dict(typemaps.values()).keys())

declare_re = r"extern lv_obj_t.\*\s*(\w+)"
NAME_SIZE = 12

# Configuration variables
working_dir = './'
include_file = "ui/ui.h"
all_symbols = False
binding_name = "LvglBinding"
constant_prefix = "ui_"
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
    if verbose: print(settings())

    process_include()

    layout.append([sg.Multiline(notes, size=(None, 5), k='notes')])
    layout.append([sg.Button('Save', button_color=('white', 'black')),
            sg.Button('Exit', button_color=('white', 'black')),
            sg.Button('Help', button_color=('white', 'black'))])
    window = sg.Window('Produce / Consume Configuration - UI Bind - Version ' + version, layout = layout, font="_ 14", finalize=True)
    
    # AFTER the window is finalized
    load_config(window)

    window['notes'].update(notes)

    while True:
        event, values = window.read()
        if event == sg.WINDOW_CLOSED or event == 'Exit':
            break

        if event == 'Save':
            save(values)

        if event == 'Help':
            sg.popup_scrolled(helptext + "\n" + TypeMaps() + "\n" + usage + '\n' + settings(), title="Help", size={90, 30})

    window.close()

#
# Action functions
#

def process_include():
    if verbose: print('Processing include file: ' + working_dir + include_file)
    try:
        f = open(working_dir + include_file)
    except Exception as e:
        error_exit(e)
    contents = f.readlines()
    f.close()

    for line in contents:
        global notes
        match = re.search(declare_re, line)
        if match:
            variable = match.group(1)
            name = get_symbol_name(variable)
            if all_symbols:
                add_variable(variable)
            elif name[0:3] in typemaps:
                add_variable(variable)
            else:
                notes += "'" + name + "' name prefix not supported\n"

def load_config(window):
    global notes
    if verbose: print('Loading config file: ' + working_dir + binding_name + ".json")
    try:
        f = open(working_dir + binding_name + ".json")
        found = json.load(f)
        for key, value in found['values'].items():
            try:
                if verbose: print('  Loading: ' + key)
                if len(key) > 4:
                    window[key].update(value)
            except Exception as e: 
                print("EX 1:" + str(e))
                notes += key[0:-4] + " Not in current code. Skipping\n"
                #print(notes)
        f.close()
    except Exception as e:
        #print(e)
        print('Warning "' + binding_name + '.json" not found')

def save(values):
    jdata = {}
    jdata['variables'] = variables
    jdata['values'] = values
    if verbose: print('Saving config file: ' + working_dir + binding_name + ".json")
    f = open(working_dir + binding_name + ".json", "w")
    f.write(json.dumps(jdata, indent=2))
    f.close()

    if verbose: print('Saving output file: ' + working_dir + binding_name + ".cpp")
    f = open(working_dir + binding_name + ".cpp", "w")
    f.write("/** " + binding_name + ".cpp   Generated " + str(date.today()) +
'''
 *
 * Generated using ESP-Object-Messaging: tools/ui_bind.py,
 '''
+ '* Version ' + version +
 ''' 
 * Do not edit manually.
 * 
 */        
'''
    )
    f.write('#include "LvglHost.h"\n')
    f.write('#include "' + include_file + '"\n')
    f.write('#include "LvglBinding.h"\n\n')
    f.write("void LvglBindingInit(LvglHost& host)\n{\n")
    for variable in variables:
        write_ProduceConsume(f, variable, values[variable + '_name'], 
          values[variable + '_pro'], values[variable + '_con'], 
          values[variable + '_type'], values[variable + '_evt'])
    f.write("}\n")
    f.write("void LvglGroupInit(lv_group_t* group)\n{\n")
    for variable in variables:
        write_GroupInit(f, variable, values[variable + '_name'], 
          values[variable + '_grp'])
    f.write("}\n")
    f.close()
#
# Utility functions
#

def sg_name(name):
    dots = NAME_SIZE-len(name)-2
    return sg.Text(name + ' ' + ' '*dots, size=(NAME_SIZE,1), justification='l',pad=(0,0))

def get_symbol_name(variable):
    return variable.replace(constant_prefix, "")

def add_variable(variable):
    name = get_symbol_name(variable)
    if name[0:3] in typemaps:
        # prefix matches, use as prefix name and use after-name for prefix
        prefix_name = name
        name = prefix_name[3:]
    else:
        # No prefix match. Configure to use "label" prefix
        prefix_name = 'lbl'
    if verbose: print('  Adding ' + variable + ' as ' + name)
    variables.append(variable)
    choices = [sg_name(variable[3:]), 
        sg.Input(k=variable + '_name', default_text=name.lower(), size=9, font="_10",),
        sg.Combo(k=variable + '_type', default_value=typemaps[prefix_name[0:3]][0], values=types, font="_ 10"), 
        sg.Checkbox('Consume', k=variable + '_con'),
        sg.Checkbox('Produce', k=variable + '_pro'), 
        sg.Combo(k=variable + '_evt', default_value=typemaps[prefix_name[0:3]][1], values=events, font="_ 10"),
        sg.Checkbox('Group', k=variable + '_grp')]
    layout.append(choices)

def write_ProduceConsume(f, variable, name, produce, consume, type, event):
    if verbose: 
        print('  Writing ' + variable + ' (' + name + ')' , end=' ')
        if produce: print('Produce', end=' ')
        if consume: print('Consume', end=' ')
        print()
    body = '("' + name + '", ' + variable + ", " + type 
    if produce: 
        f.write('    host.AddProducer' + body + ", " + event + ");\n")
    elif verbose:
        f.write('    // AddProducer' + body + ", " + event + ");\n")
    if consume: 
        f.write('    host.AddConsumer' + body + ");\n")
    elif verbose:
        f.write('    // AddConsumer' + body + ");\n")

def write_GroupInit(f, variable, name, group):
    if verbose: 
        print('  Writing ' + variable + ' (' + name + ')' , end=' ')
        if group: print('Group', end=' ')
        print()
    if group: 
        f.write('    lv_group_add_obj(group, ' + variable + ');\n')
    elif verbose:
        f.write('    // Add group(' + variable + ");\n")

def settings():
  return '''Settings
  working_dir:     ''' + working_dir + '''
  all_symbols:     ''' + str(all_symbols) + '''
  include_file:    ''' + include_file + ' (' + working_dir + include_file + ''')
  binding_name:    ''' + binding_name + ' (' + working_dir + binding_name + ''')
    Configuration: ''' + working_dir + binding_name + '''.json
    Generated:     ''' + working_dir + binding_name + '''.cpp
  Constant prefix: ''' + constant_prefix

def help():
    print(helptext)
    print(TypeMaps())
    print(usage)
    print(settings())

usage = '''Usage: ui_bind.py [-w <working_dir>][-i <include_file>][-b <binding_name>][-p <prefix>][-v] [-h]
     -a: Present all lv_obj symbols as opposed to only those with recognized name prefix
     -s: Report settings to stdout
     -v: Verbose
     -h: Show this help
'''

def TypeMaps():
    s ="Control type prefixes:\n"
    for t in typemaps:
        s += '    ' + t + '  ' + typemaps[t][0] + '\n'
    return s

def error_exit(err):
    # output error, and return with an error code
    print (str(err))
    print(settings())
    print(usage)
    exit()

# process command line arguments
arglist = sys.argv[1:]

# Options
options = "w:i:b:p:vhas"
# Long options doesn't seem to work ... avoid for now
long_options = [] # ["Help", "working_dir=", "include=", "binding_name=", "prefix=", "verbose", help]

try:
    # Parsing argument
    args, values = getopt.getopt(arglist, options, long_options)

    # checking each argument
    for arg, argv in args:

        if arg in ("-h", "--help"):
            help()
            exit()
        if arg in ("-v", "--verbose"):
            verbose = True
        elif arg in ("-s"):
            print(settings())
        elif arg in ("-a"):
            all_symbols = True
        elif arg in ("-w", "--working_dir"):
            working_dir = argv
            if not working_dir.endswith('/') and not working_dir.endswith('\\'):
                working_dir += '/'
            print ("Working directory: " + working_dir)
        elif arg in ("-i", "--include"):
            include_file = argv
            print ("Include file: " + include_file)
        elif arg in ("-b", "--binding_name"):
            binding_name = argv
            print ("Binding name: " + binding_name)
        elif arg in ("-p", "--prefix"):
            constant_prefix = argv
            print ("Constant prefix: " + constant_prefix)

except getopt.error as err:
    error_exit(err)

main()