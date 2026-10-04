#!/usr/bin/env python3
"""Invented cover art for the UI preview (no real game artwork)."""
import math, random, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
font = sys.argv[2]
games = [("PRVW01","Island Adventure",(18,140,220),(250,210,120)),("PRVW02","Comet Racers",(90,20,150),(255,120,40)),
         ("PRVW03","Haunted Manor",(20,30,70),(110,220,120)),("PRVW04","Sunny Shores",(40,170,255),(255,230,90)),
         ("PRVW05","Sky Sail",(30,110,200),(240,250,255)),("PRVW06","Mech Hunter",(60,20,20),(255,140,40)),
         ("PRVW07","Kart Party",(30,150,60),(255,60,60)),("PRVW08","Garden Critters",(60,140,60),(255,200,230))]
W,H = 400,560
for gid,title,top,accent in games:
    rnd = random.Random(gid)
    im = Image.new('RGB',(W,H))
    d = ImageDraw.Draw(im)
    for y in range(H):
        t=y/H; c=tuple(int(top[i]*(1-t)+top[i]*0.35*t) for i in range(3)); d.line([(0,y),(W,y)],fill=c)
    for _ in range(14):
        r=rnd.randint(20,120); x=rnd.randint(-40,W+40); y=rnd.randint(120,H+40)
        a=tuple(min(255,int(v*rnd.uniform(0.6,1.1))) for v in accent)
        d.ellipse([x-r,y-r,x+r,y+r],fill=a)
    im = im.filter(ImageFilter.GaussianBlur(1.2)); d = ImageDraw.Draw(im)
    f = ImageFont.truetype(font, 54)
    words = title.split(); lines=[]; line=""
    for w in words:
        trial=(line+" "+w).strip()
        if d.textlength(trial,font=f) > W-60 and line: lines.append(line); line=w
        else: line=trial
    lines.append(line)
    y=36
    for l in lines:
        w=d.textlength(l,font=f); d.text(((W-w)/2+3,y+3),l,font=f,fill=(0,0,0)); d.text(((W-w)/2,y),l,font=f,fill=(255,255,255)); y+=62
    im.save(out/f"{gid}.png")
print("covers in", out)
