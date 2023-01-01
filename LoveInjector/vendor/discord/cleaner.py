import os
import re

for file in os.listdir('.'):
  if file.endswith('.cpp') or file.endswith('.c'):
    with open(file, 'r') as f:
      lines = f.readlines()

    for i, line in enumerate(lines):
      if  line.startswith('#define _CRT_SECURE_NO_WARNINGS'):
        lines.pop(i)

    lines.insert(0, '#include "ccpch.h"\n')
    lines.insert(1, '#pragma warning ( disable : 4005 )\n')

    with open(file, 'w') as f:
      f.writelines(lines)

print('Done!')