#!/usr/bin/env python3
"""Original board press control 1 (0x1161D0 entry, 0x12FC60/0x12FC80/0x12FE98 and every
phase/helper routine) conformance against engine/board_press.hpp."""
from boardpress_oracle import run_boardpress_oracle
run_boardpress_oracle('control',['001161D0','0012FC60','0012FC80','0012FE98','0012FEC8','0012FFF8','00130228','001303E0','001304D0',
    '001304E0','001306B0','001307B8','001308D8','00130DD0','00131200','00131428','001313A8','00131348','001162C8','0031BE50','0031C228'])
