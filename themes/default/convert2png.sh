#!/bin/sh

for i in `\ls *.svg`; do 
    IMAGE=${i%.svg}
    # inkscape --without-gui --file=$i --export-png=$IMAGE.png
    rsvg -f png ${i} $IMAGE.png
done
