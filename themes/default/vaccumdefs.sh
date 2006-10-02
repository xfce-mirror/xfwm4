#!/bin/sh

for i in `\ls *.svg`; do 
    inkscape --without-gui --vacuum-defs --file=$i
done

