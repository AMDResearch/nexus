"""
GFX1103 (RDNA3) Backend

gfx1103 (Phoenix) is the RDNA3 iGPU in Ryzen 7040/8040-series APUs, e.g. the
Radeon 780M in a Ryzen 9 7940HS. It is GFX11 hardware, so it would share
aqlprofile's GFX11 counter-block layout with gfx1100.

In practice it has no hardware counters: ROCm 7.2.4 ships counter definitions
for gfx1100/1101/1102 and gfx1150/1151 but none for gfx1103, so
`rocprofv3 --list-avail` reports no counters and asking for even one hangs the
profiled process. gfx1103 is therefore absent from every architecture list in
counter_defs.yaml (see the note there) and metrix runs it in time-only mode.

This backend exists for the device specs, which do differ from gfx1100: Phoenix
reads from DDR5 system memory rather than GDDR6 (see device_info.py).
"""

from .device_info import query_device_specs
from .gfx1100 import GFX1100Backend


class GFX1103Backend(GFX1100Backend):
    """AMD RDNA3 (GFX1103 / Phoenix) backend - same counter blocks as gfx1100."""

    def _get_device_specs(self):
        return query_device_specs("gfx1103")
