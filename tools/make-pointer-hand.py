#!/usr/bin/env python3
"""Porpoise - the Revolution look's pointer: a hand pointing up, drawn here
from simple shapes (an original drawing). Run from the repository root:
    python3 tools/make-pointer-hand.py
The fingertip (the spot it points at) is at 41% across, 2% down.
Copyright (C) 2026 Ruben (Project Porpoise)
SPDX-License-Identifier: GPL-3.0-or-later
"""
import cairosvg
shapes = '''
<rect x="31" y="6" width="17" height="62" rx="8.5"/>
<path d="M24 58 Q24 50 33 50 L74 52 Q84 54 84 66 L84 96 Q84 116 64 118 L44 118 Q26 116 24 98 Z"/>
<circle cx="56" cy="58" r="10"/>
<circle cx="69" cy="61" r="9.5"/>
<circle cx="79" cy="68" r="8.5"/>
<path d="M27 84 Q16 80 10 70 Q6 62 12 59 Q18 57 24 66 L30 74 Z"/>
<rect x="34" y="112" width="40" height="18" rx="6"/>
'''
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 136" width="192" height="272">
<g fill="#26303D" stroke="#26303D" stroke-width="7" stroke-linejoin="round">{shapes}</g>
<g fill="#FFFFFF" stroke="none">{shapes}</g>
<rect x="34" y="112" width="40" height="18" rx="6" fill="#4CC3F0"/>
<rect x="34" y="112" width="40" height="5" rx="2.5" fill="#9BE0FA"/>
<g fill="none" stroke="#B8C2CE" stroke-width="2.2" stroke-linecap="round">
<path d="M49 64 Q50 70 48 74"/><path d="M62 66 Q63 71 61 75"/><path d="M74 71 Q75 75 73 78"/>
<path d="M31 80 Q36 84 40 82"/>
</g>
<path d="M35 12 Q35 9 38 9" stroke="#E8EEF5" stroke-width="3" fill="none" stroke-linecap="round"/>
</svg>'''
open('hand.svg','w').write(svg)
cairosvg.svg2png(bytestring=svg.encode(), write_to='assets/ui/pointer-hand.png', output_width=192, output_height=272)
print('wrote assets/ui/pointer-hand.png')
