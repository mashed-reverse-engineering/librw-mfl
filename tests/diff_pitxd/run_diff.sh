#!/bin/sh
# usage: run_diff.sh files.txt -> SAME/DIFF per PI file (native 0x16 files print nothing)
i=0
while read f; do
  i=$((i+1))
  ./pitxddump "$f" > d/$i.lib 2>/dev/null
  python3 -I orig_reader.py dump "$f" > d/$i.orig 2>d/$i.err
  [ -s d/$i.orig ] || continue
  if cmp -s d/$i.lib d/$i.orig; then echo "SAME $i $f"; else echo "DIFF $i $f"; fi
done < "$1"
