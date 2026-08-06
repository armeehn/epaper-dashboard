#!/usr/bin/env python3
# Emulate the Arduino IDE's prototype generation: hoist a prototype for every
# top-level function to just above the FIRST function definition. This
# reproduces "'Type' does not name a type" errors that a plain g++ compile of
# the .ino misses.
import re, sys

src = open(sys.argv[1]).read()
lines = src.split("\n")

def strip_for_depth(s):
    s = re.sub(r'//.*$', '', s)
    s = re.sub(r'"(\\.|[^"\\])*"', '""', s)
    s = re.sub(r"'(\\.|[^'\\])'", "''", s)
    return s

KEYWORDS = ("if", "for", "while", "switch", "else", "return", "do", "enum",
            "struct", "class", "typedef", "template", "using", "namespace", "extern")

depth = 0
first_func_line = None
protos = []
i = 0
while i < len(lines):
    line = lines[i]
    stripped = strip_for_depth(line)
    at_top = (depth == 0)
    # detect a top-level function definition beginning on this line
    if at_top and line[:1] not in ("", " ", "\t", "#", "/", "*", "}") \
       and not line.lstrip().startswith(KEYWORDS) \
       and "(" in line and "=" not in line.split("(")[0]:
        # join logical signature until we see '{' or ';' at top level
        sig_lines = [line]
        j = i
        joined = stripped
        while "{" not in joined and ";" not in joined and j + 1 < len(lines):
            j += 1
            sig_lines.append(lines[j])
            joined += " " + strip_for_depth(lines[j])
        if "{" in joined and ";" not in joined.split("{")[0]:
            sig = "\n".join(sig_lines)
            sig = sig[:sig.rindex("{")].rstrip()
            # drop a leading 'static' so the hoisted prototype is a plain decl
            proto = sig.strip() + ";"
            protos.append(proto)
            if first_func_line is None:
                first_func_line = i
            # advance past the whole function body
            depth += joined.count("{") - joined.count("}")
            i = j + 1
            while depth > 0 and i < len(lines):
                depth += strip_for_depth(lines[i]).count("{") - strip_for_depth(lines[i]).count("}")
                i += 1
            continue
    depth += stripped.count("{") - stripped.count("}")
    i += 1

out = lines[:first_func_line] + ["// ==== hoisted prototypes (Arduino IDE emulation) ===="] \
      + protos + [""] + lines[first_func_line:]
open(sys.argv[2], "w").write("\n".join(out))
print(f"hoisted {len(protos)} prototypes above line {first_func_line+1}")
