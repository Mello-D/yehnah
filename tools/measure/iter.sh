#!/bin/bash
S=${MEAS_DIR:-$(cd "$(dirname "$0")" && pwd)}
cd /home/dave_cathcart/JIANT
rm -rf $S/nolim && mkdir -p $S/nolim && cp -r firmware tests $S/nolim/ && sed -i 's/#define LIM_T 18000/#define LIM_T 1000000000/' $S/nolim/firmware/src/fx.c && cp $S/meas.c $S/nolim/tests/meas.c
cd $S/nolim && PATH=$S/bin:$PATH cc -O2 -w -DYEHNAH_ANIMAL=1 -DYEHNAH_NOISE=1 -I/home/dave_cathcart/JIANT/build/gen -Ifirmware/src -Itests tests/meas.c -o $S/meas_nolim -lm || exit 1
cd $S && python3 mixlvl.py $1 | tail -1
