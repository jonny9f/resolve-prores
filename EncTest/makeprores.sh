#⁄bin/bash
ffmpeg -y -f lavfi -i testsrc=duration=10:size=1920x1080:rate=25  \
-c:v prores \
-profile:v 2 \
-vendor apl0 \
-pix_fmt yuv422p10 \
-color_trc bt709 \
-color_primaries bt709 \
-colorspace bt709 \
-color_range tv  \
-movflags +write_colr \
output.mov