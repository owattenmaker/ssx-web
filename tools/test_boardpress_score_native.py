#!/usr/bin/env python3
"""Original board-press scoring (0x1199F8 press, 0x119AD8/0x119898 pivot, 0x119A38 end) conformance."""
from boardpress_oracle import run_boardpress_oracle
run_boardpress_oracle('score',['001199F8','00119AD8','00119898','00119A38','001176F8','00117838'],engine=('ground_motion.cpp',))
