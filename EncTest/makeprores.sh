#⁄bin/bash
ffmpeg -y -f lavfi -i testsrc=duration=10:size=1920x1080:rate=25  \
-c:v prores \
-profile:v 4 \
-vendor apl0 \
-pix_fmt yuv444p10 \
proRes_output.mov