# extract module strings from README.rst

import sys, re
import commands
from datetime import datetime
import json
import argparse

from align_string_proportional import word_wrap
from rbf_read import extent_func, rbf_init_font
rbf_init_font("../../data/fonts/argnor23.rbf")

class NullIO():
    def write(self, s):
        pass


def run(cmd):
    return commands.getstatusoutput(cmd)[1]

# see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=37506 for how to place some strings in a custom section
# (you must declare all strings as variables, not only the pointers)

def c_repr(name):
    if "\n" in name:
        s = "\n"
        for l in name.split("\n"):
            s += "    %s\n" % ('"%s\\n"' % l.replace('"', r'\"'))
        return s
    else:
        return '"%s"' % name.replace('"', r'\"')

strings = []
def add_string(name, value, fp):
    a = chr(ord('a') + len(strings))
    print >> fp, "static char __module_string_%s_name [] MODULE_STRINGS_SECTION = %s;" % (a, c_repr(name))
    print >> fp, "static char __module_string_%s_value[] MODULE_STRINGS_SECTION = %s;" % (a, c_repr(value))

    strings.append((name, value))

def declare_string_section(fp):
    print >> fp, ''
    print >> fp, 'MODULE_STRINGS_START()'
    for i, s in enumerate(strings):
        a = chr(ord('a') + i)
        print >> fp, "    MODULE_STRING(__module_string_%s_name, __module_string_%s_value)" % (a, a)
    print >> fp, 'MODULE_STRINGS_END()'

def setup_argparse():
    parser = argparse.ArgumentParser(description='Post process README.rst file')
    parser.add_argument('--json', help='Produce json output', nargs='?')
    parser.add_argument('--header', help='Produce C header output', nargs='?')
    return parser


parser = setup_argparse()
args   = parser.parse_args()

if(args.header is not None):
    fp_header = open(args.header, 'w')
else:
    fp_header = NullIO()

inp = open("README.rst").read().replace("\r\n", "\n")
lines = inp.strip("\n").split("\n")
title = lines[0]

used_lines = []
for l in lines[2:]:
    if l.startswith("..") or l.strip().startswith(":"):
        continue
    
    used_lines.append(l)

inp = "\n".join(used_lines)
inp = inp.split("\n\n")

# extract user metadata from RST meta tags
fields = {}
fields["Name"] = title
add_string("Name", title, fp_header)
for l in lines[2:]:
    l = l.strip()
    m = re.match("^:([^:]+):(.+)$", l)
    if m:
        name = m.groups()[0].strip()
        value = m.groups()[1].strip()
        if value.startswith("<") and value.endswith(">"):
            continue
        add_string(name, value, fp_header)
        fields[name] = value

if(args.json is not None):
    fp_json = open(args.json, 'w')
    print >> fp_json, "%s" % (json.dumps(fields, indent=4))

if "Author" not in fields and "Authors" not in fields:
    print >> sys.stderr, "Warning: 'Author/Authors' tag is missing. You should tell the world who wrote your module ;)"

if "License" not in fields:
    print >> sys.stderr, "Warning: 'License' tag is missing. Under what conditions we can use your module? Can we publish modified versions?"

if "Summary" not in fields:
    print >> sys.stderr, "Warning: 'Summary' tag is missing. It should be displayed as help in the Modules tab."

# extract readme body:
# intro -> "Description" tag;
# each section will become "Help page 1", "Help page 2" and so on

# render the RST as html -> txt without the metadata tags
txt = run('cat README.rst | grep -v -E "^:([^:])+:.+$" | rst2html --no-xml-declaration | python ../html2text.py -b 700')

desc = ""
last_str = "Description"
help_page_num = 0
lines_per_page = 0
for p in txt.strip("\n").split("\n")[2:]:
    if p.startswith("# "): # new section
        help_page_num += 1
        add_string(last_str, desc, fp_header)
        desc = ""
        last_str = "Help page %d" % help_page_num
        lines_per_page = 0
        p = p[2:].strip()
    desc += "%s\n" % p
    lines_per_page += 1
    if lines_per_page > 18:
        print >> sys.stderr, "Too many lines per page\n"
        exit(1)

add_string(last_str, desc, fp_header)

# extract version info
# (prints the latest changeset that affected this module)
last_change_info = run("LC_TIME=EN hg log . -r $(basename $(hg id -n) +):0 -l 1 --template '{date|hgdate}\n{node|short}\n{author|user}\n{desc|strip|firstline}'")
if len(last_change_info):
    last_change_date, last_changeset, author, commit_msg = last_change_info.split("\n")
    split = last_change_date.split(" ")
    seconds = float(split[0])
    last_change_date = datetime.utcfromtimestamp(seconds).strftime("%Y-%m-%d %H:%M:%S UTC")
    
    # trim changeset to 7 chars, like Bitbucket does
    last_changeset = last_changeset[:7]
    
    # trim commit msg to 700px
    size = extent_func(commit_msg)[0]
    if size > 700:
        new_size = 0
        new_msg = ""
        for c in commit_msg:
            new_size += extent_func(c)[0]
            if new_size > 700:
                break
            new_msg += c
        commit_msg = new_msg + "..."
        
    add_string("Last update", "%s on %s by %s:\n%s" % (last_changeset, last_change_date, author, commit_msg), fp_header)

build_date = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")
build_user = run("echo `whoami`@`hostname`")

add_string("Build date", build_date, fp_header)
add_string("Build user", build_user, fp_header)

declare_string_section(fp_header)
