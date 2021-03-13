for /F %%f in (emoji.txt) do (
    magick convert -border 1x1 -bordercolor none -background none -size 126x126 ../twemoji/assets/svg/%%f.svg %%f.png
)
magick montage *.png -background none -geometry 128x128 -tile 8x8 ../spritesheet.png
