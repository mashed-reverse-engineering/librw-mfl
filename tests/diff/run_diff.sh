#!/bin/sh
# usage: run_diff.sh listfile  -> per-file diff status (plbox lines and normal hashes handled separately)
i=0
while read f; do
  i=$((i+1))
  ./bspdump "$f" > d/$i.lib 2>/dev/null
  python3 -I orig_reader.py dump "$f" > d/$i.orig 2>d/$i.err
  cp d/$i.orig d/$i.o2
  if [ ! -s d/$i.lib ] || [ ! -s d/$i.o2 ]; then echo "EMPTY $i $f"; elif cmp -s d/$i.lib d/$i.o2; then echo "SAME $i $f"; else
    # same after masking normal hash?
    sed -E 's/ n [0-9a-f]{16} / n X /' d/$i.lib > d/$i.l3; sed -E 's/ n [0-9a-f]{16} / n X /' d/$i.o2 > d/$i.o3
    if cmp -s d/$i.l3 d/$i.o3; then echo "NORMALSONLY $i $f"; else echo "DIFF $i $f"; fi
  fi
done < "$1"
