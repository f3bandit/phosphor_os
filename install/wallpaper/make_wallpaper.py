# Draws install/wallpaper/phosphor-wallpaper.png, the picture behind MiSTer's own menu (1920x1080).
# python3 make_wallpaper.py   (Inter and DejaVu Sans Mono fonts; "mock" also draws a mockup with the OSD)
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import sys
W,H=1920,1080
ACC=(255,179,71); BG=(14,14,16); DIM=(141,138,133); FAINT=(70,68,66); LINE=(42,42,49)
img=Image.new("RGB",(W,H),BG)
# soft glow in the middle, like a warm tube
glow=Image.new("L",(W,H),0); g=ImageDraw.Draw(glow); g.ellipse((W*0.15,H*0.05,W*0.85,H*0.95),fill=34)
glow=glow.filter(ImageFilter.GaussianBlur(220))
img=Image.composite(Image.new("RGB",(W,H),(40,30,20)),img,glow)
d=ImageDraw.Draw(img)
# scanlines, very faint
for y in range(0,H,4): d.line((0,y,W,y),fill=(10,10,12))
mono=lambda s: ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",s)
inter=lambda s,w="Regular": ImageFont.truetype(f"/usr/share/fonts/opentype/inter/Inter-{w}.otf",s)
def spaced(xy,t,f,fill,sp):
    x,y=xy
    for ch in t: d.text((x,y),ch,font=f,fill=fill); x+=f.getlength(ch)+sp
# wordmark, top left (the shell's place at 1080p)
spaced((84,66),"PHOSPHOR",mono(30),ACC,10)
# element tile, bottom right: 15 / P / Phosphor
x0,y0,s=W-84-150,H-84-170,150
d.rounded_rectangle((x0,y0,x0+s,y0+170),radius=14,outline=ACC,width=3)
d.text((x0+16,y0+12),"15",font=inter(26,"SemiBold"),fill=ACC)
f=inter(86,"SemiBold"); tw=f.getlength("P"); d.text((x0+(s-tw)/2,y0+40),"P",font=f,fill=ACC)
f=inter(20); tw=f.getlength("Phosphor"); d.text((x0+(s-tw)/2,y0+136),"Phosphor",font=f,fill=DIM)
# bottom left: where you are, and the way back
spaced((84,H-84-70),"MISTER MENU",mono(20),DIM,6)
d.text((84,H-84-36),"Back to Phosphor:  hold the menu button, or  System  ›  Launch Phosphor",font=inter(26),fill=(200,196,190))
img.save("phosphor-wallpaper.png")
if len(sys.argv)>1:
    m=img.copy(); md=ImageDraw.Draw(m)
    ow,oh=520,620; ox,oy=(W-ow)//2,(H-oh)//2
    md.rectangle((ox,oy,ox+ow,oy+oh),fill=(0,0,0),outline=(90,90,90),width=2)
    of=mono(24)
    for i,t in enumerate(["  MiSTer          System","","  Core","  Scripts","  Define joystick buttons","  Launch Phosphor","  ...","","  (MiSTer's OSD, for the mockup)"]):
        md.text((ox+20,oy+30+i*40),t,font=of,fill=(230,230,230) if i!=8 else (120,120,120))
    m.save("mock-with-osd.png")
