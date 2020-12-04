@ECHO OFF

set FOLDER="%~1"
set MP4FILE="%~2"

cd %FOLDER%

ffmpeg -y -framerate 30 -f image2 -i temp/frame%%04d.png -an -vcodec libx264 -g 300 -preset veryslow -tune zerolatency -profile:v baseline -level 3.0 -crf 28  -pix_fmt yuv420p   %MP4FILE%

rd /s /q temp