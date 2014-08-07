# Sentence-case is preferred for ML menu, https://bitbucket.org/hudson/magic-lantern/commits/63fb88cbacc50c65f261943da37a68953f93611e#comment-1143179
# This script fixes it everywhere.

import os, sys, re, string

exceptions = ["Canon", "Magic", "Lantern", "Dual ISO", "Zoom In", "Magic Zoom", "Hz", "H264.ini"]
exceptions_lower = [ex.lower() for ex in exceptions]

def ireplace(old, repl, text):
    return re.sub('(?i)'+re.escape(old), lambda m: repl, text)

def fix_case_word(word):
    if len(word) <= 1:
        return word
    if word.lower() in exceptions_lower:
        return [ex for ex in exceptions if ex.lower() == word.lower()][0]
    if word[0] == word[0].upper() and word[1:] == word[1:].lower():
        return word.lower()
    return word

def fix_case(name):
    words = name.split(" ")
    fixed = [words[0]]
    for w in words[1:]:
        
        # skip multiple spaces
        if len(w) == 0:
            continue
        
        # don't force lowarcase after ":"
        if fixed[-1][-1] == ":":
            fixed.append(w)
            continue
        
        fixed.append(fix_case_word(w))
    
    fixed = " ".join(fixed)
    
    # process multi-word exceptions (fixme: might do incorrect replacements, unlikely)
    for ex in exceptions:
        if " " in ex:
            fixed = ireplace(ex, ex, fixed)
    
    return fixed

src_dirs = ["../../src/"] + ["../../modules/" + dir + "/" for dir in os.listdir("../../modules/") if os.path.isdir("../../modules/" + dir)]
sources = []
for dir in src_dirs:
    sources += [dir + f for f in os.listdir(dir) if f.endswith(".c")]

for src in sources:
    lines = open(src).readlines()
    modified = []
    for line in lines:
        
        # scan for named menu entries, and change them to "Sentence case"
        pattern = '( *)\.name = "([^"]*)",'
        m = re.match(pattern, line)
        if m:
            name = m.groups()[1]
            fixed = fix_case(name)
            print name, "=>", fixed
            if name != fixed:
                print line
                line = re.sub(pattern, '\\1.name = "%s",' % fixed, line)
                print line
        modified.append(line)
    
    # save changes, if any
    if modified != lines:
        f = open(src, "w")
        for l in modified:
            print >> f, l,
        f.close()
