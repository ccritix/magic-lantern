# This script fixes menu indentation (aligns all entries at "=")

import os, sys, re, string

def fix_indent(line):
    pattern = '( *)\.([a-z_0-9]+) *= *(.*)'
    m = re.match(pattern, line)
    spaces, field, value = m.groups()
    return "%s.%-10s = %s\n" % (spaces, field, value)

src_dirs = ["../../src/"] + ["../../modules/" + dir + "/" for dir in os.listdir("../../modules/") if os.path.isdir("../../modules/" + dir)]
sources = []
for dir in src_dirs:
    sources += [dir + f for f in os.listdir(dir) if f.endswith(".c")]

for src in sources:
    lines = open(src).readlines()
    modified = []
    last_indent = None
    
    is_menu_struct = False
    for line in lines:
        
        # scan for menu structure fields (identified by .name and terminated by "}")
        pattern_name = '( *)\.(name) *= *"'
        pattern_any = '( *)\.([a-z_0-9]+) *= *'
        
        fixed = line
        m = re.match(pattern_any if is_menu_struct else pattern_name, line)
        if m:
            is_menu_struct = True
            fixed = fix_indent(line)
            last_indent = fixed.index("=")
        else:
            if is_menu_struct and line.strip().startswith('"'):
                fixed = " " * (last_indent + 2) + line.strip() + "\n"
        if line.strip().startswith("}"):
            is_menu_struct = False
            last_indent = None

        if line != fixed:
            print fixed.strip("\n")
            line = fixed
        
        modified.append(line)
    
    # save changes, if any
    if modified != lines:
        f = open(src, "w")
        for l in modified:
            print >> f, l,
        f.close()
