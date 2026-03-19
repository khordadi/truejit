#!/usr/bin/env bash

export SPEC_ROOT=/tmp/spec

# 511.povray_r
cd $SPEC_ROOT/benchspec/CPU/511.povray_r/data
echo "SPEC-benchmark-test.ini SPEC-benchmark-test.pov" >test/input/control
echo "SPEC-benchmark-train.ini SPEC-benchmark-train.pov" >train/input/control
echo "SPEC-benchmark-ref.ini SPEC-benchmark-ref.pov" >refrate/input/control
cp -r all/* test/
cp -r all/* train/
cp -r all/* refrate/

# 519.lbm_r
cd $SPEC_ROOT/benchspec/CPU/519.lbm_r/data/
# for each lbm.in file, copy it to control file
find . -type f -name "lbm.in" -exec bash -c 'cp "$1" "${1/lbm.in/control}"' -- {} \;

# 523.xalancbmk_r
cd $SPEC_ROOT/benchspec/CPU/523.xalancbmk_r/data/
echo "test.xml xalanc.xsl" >test/input/control
echo "allbooks.xml xalanc.xsl" >train/input/control
echo "t5.xml xalanc.xsl" >refrate/input/control

# 525.x264_r
cd $SPEC_ROOT/benchspec/CPU/525.x264_r/data/
echo "--dumpyuv 50 --frames 156 -o BuckBunny_New.264 BuckBunny.264 1280x720" >test/input/control
echo "--dumpyuv 50 --frames 156 -o BuckBunny_New.264 BuckBunny.264 1280x720" >train/input/control
echo "--pass 1 --stats x264_stats.log --bitrate 1000 --frames 1000 -o BuckBunny_New.264 BuckBunny.264  1280x720" >refrate/input/control

# 531.deepsjeng_r
cd $SPEC_ROOT/benchspec/CPU/531.deepsjeng_r/data/
echo "ref.txt" >refrate/input/control
echo "test.txt" >test/input/control
echo "train.txt" >train/input/control

# 541.leela_r
cd $SPEC_ROOT/benchspec/CPU/541.leela_r/data/
echo "ref.sgf" >refrate/input/control
echo "test.sgf" >test/input/control
echo "train.sgf" >train/input/control

# 548.exchange2_r
cd $SPEC_ROOT/benchspec/CPU/548.exchange2_r/data/
cp -r all/* test/
cp -r all/* train/
cp -r all/* refrate/

# 557.xz_r
cd $SPEC_ROOT/benchspec/CPU/557.xz_r/data/
cp -r all/* test/
cp -r all/* train/
cp -r all/* refrate/
cp -r all/* refspeed/
# take a backup of the original control files; then empty them
find . -type f -name "control" -exec bash -c 'cp "$1" "${1/control/control.orig}"' -- {} \;
# in control file replace '.tar' with '.tar.xz' and '.combined' with '.combined.xz'
find . -type f -name "control" -exec sed -i 's/\.tar/\.tar\.xz/g' {} \;
find . -type f -name "control" -exec sed -i 's/\.combined/\.combined\.xz/g' {} \;
