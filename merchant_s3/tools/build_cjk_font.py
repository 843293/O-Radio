"""Generate a fixed 16x20 CJK bitmap font; requires Pillow and Noto Sans CJK."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import argparse
p=argparse.ArgumentParser();p.add_argument('--font',default='/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc');args=p.parse_args()
# TTC face 2 is Simplified Chinese in the installed Noto CJK collection.
f=ImageFont.truetype(args.font,16,index=2)
ranges=[(0x3000,0x3100),(0x3400,0xA000),(0xFF00,0xFFF0)]
out=Path(__file__).resolve().parents[1]/'include/cjk_font.h'
with out.open('w') as w:
    w.write('// Generated from Noto Sans CJK SC, SIL OFL 1.1; see fonts/OFL.txt.\n#pragma once\n#include <stdint.h>\n')
    w.write('static const uint8_t kCjkBitmap[][40] = {\n')
    for lo,hi in ranges:
        for cp in range(lo,hi):
            im=Image.new('L',(16,20));ImageDraw.Draw(im).text((0,17),chr(cp),font=f,fill=255,anchor='ls')
            data=[]
            for y in range(20):
                for x0 in (0,8):
                    data.append(sum((1<<(7-i)) for i in range(8) if im.getpixel((x0+i,y))>=96))
            w.write('{'+','.join(str(x) for x in data)+'},\n')
    w.write('};\ninline const uint8_t* cjkGlyph(uint32_t cp) {\n')
    base=0
    for lo,hi in ranges:
        w.write(f'  if(cp>=0x{lo:X} && cp<0x{hi:X})return kCjkBitmap[{base}+cp-0x{lo:X}];\n');base+=hi-lo
    w.write('  return nullptr;\n}\n')
print(out,base,'glyphs',base*40,'bitmap bytes')
