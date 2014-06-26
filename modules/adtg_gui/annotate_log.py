import os, sys, re

if len(sys.argv) == 1:
    print "This script looks for ADTG, CMOS and ENGIO registers in a log file, and adds comments"
    print "The registers are identified from adtg_gui.c (it parses the source code)"
    print "Usage: python annotate_log.py input.log > output.log"
    raise SystemExit

lines = open("adtg_gui.c").readlines()

engio_regs = {}
adtg_regs = {}
cmos_regs = {}

for l in lines:
    # {0xC0F0,   0x8014, 0, "DARK_LIMIT_14_12 (0x0000 - 0x0FFF) (no noticeable change)"},
    m = re.match(" *{([^,]*),([^,]*),([^,]*), *\"(.*)\"}", l)
    if m:
        dst, reg, nrzi, desc = m.groups()
        if dst.startswith("0x"):
            reg = (int(dst,16) << 16) + int(reg,16)
            #~ print dst, hex(reg), nrzi, desc
            engio_regs[reg] = desc
        elif dst.strip() == "DST_ADTG":
            reg = int(reg,16)
            adtg_regs[reg] = desc
        elif dst.strip() == "DST_CMOS":
            reg = int(reg,16)
            cmos_regs[reg] = desc

lines = open(sys.argv[1]).readlines()
lines = [l.strip("\n") for l in lines]

for l in lines:
    comments = []
    
    for r, desc in engio_regs.iteritems():
        if "%X" % r in l.upper():
            comments.append("%x: %s" % (r, desc))

    for r, desc in adtg_regs.iteritems():
        if "ADTG:[0X%04X" % r in l.upper():
            comments.append("ADTG[%x]: %s" % (r, desc))

    for r, desc in cmos_regs.iteritems():
        if "CMOS:[0X%X" % r in l.upper():
            comments.append("CMOS[%x]: %s" % (r, desc))

    if comments:
        print "%-80s ; %s" % (l, "; ".join(comments))
    else:
        print l
