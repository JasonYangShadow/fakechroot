#!/usr/bin/python3
import sys

def scandata(fname):
    mem_map = {}
    with open(fname) as f:
        line = f.readline()
        linenum = 1
        while line:
            ##process line
            if line.startswith(">>> "):
                address = line[line.rfind("address: ") + len("address: "):]
                if address in mem_map:
                    print("memory is leaked and not freed %s, Line: %d" % (mem_map[address], linenum))
                else:
                    mem_map[address] = "linenum: %d, %s" % (linenum, line)
    
            if line.startswith("<<< "):
                address = line[line.rfind("address: ") + len("address: "):]
                if address in mem_map:
                    del mem_map[address]
                else:
                    print("memory is invalid freed, %s" % line)
            line = f.readline()
            linenum += 1
    for item in mem_map.items():
        print(item[1])

if __name__ == "__main__":
    scandata(sys.argv[1])
