"""Pure-Python geometry for the grey-box tower. No Unreal imports, so it is testable with plain python3.

All units metres, building-local: origin at the south-west corner of the lobby slab, X east, Y north,
Z up. Floor f's walking surface is at z = f * floor_height. Slabs hang *below* that surface.
"""
from dataclasses import dataclass, field
import csv
import os


@dataclass(frozen=True)
class Rect:
    x: float
    y: float
    w: float
    d: float

    @property
    def x2(self):
        return self.x + self.w

    @property
    def y2(self):
        return self.y + self.d

    def overlaps(self, o):
        return self.x < o.x2 and o.x < self.x2 and self.y < o.y2 and o.y < self.y2


def subtract(rects, hole):
    """Subtract `hole` from every rect, returning axis-aligned pieces that tile the remainder."""
    out = []
    for r in rects:
        if not r.overlaps(hole):
            out.append(r)
            continue
        # left strip
        if hole.x > r.x:
            out.append(Rect(r.x, r.y, hole.x - r.x, r.d))
        # right strip
        if hole.x2 < r.x2:
            out.append(Rect(hole.x2, r.y, r.x2 - hole.x2, r.d))
        # middle band, bottom and top of the hole
        mx = max(r.x, hole.x)
        mw = min(r.x2, hole.x2) - mx
        if hole.y > r.y:
            out.append(Rect(mx, r.y, mw, hole.y - r.y))
        if hole.y2 < r.y2:
            out.append(Rect(mx, hole.y2, mw, r.y2 - hole.y2))
    return [r for r in out if r.w > 1e-6 and r.d > 1e-6]


@dataclass
class TowerSpec:
    num_floors: int = 15          # floors above the lobby; lobby is floor 0
    floor_height: float = 3.8
    slab_thickness: float = 0.25
    building_x: float = 30.0
    building_y: float = 20.0
    hoist_shaft: Rect = field(default_factory=lambda: Rect(14, 8, 3, 3))
    stairwell: Rect = field(default_factory=lambda: Rect(1, 1, 2, 4))
    stair_landing_gap: bool = True  # keep the stair hole open on every floor (concrete shell)


@dataclass(frozen=True)
class Box:
    """An axis-aligned box to spawn: centre (m) and full size (m), plus semantic tags."""
    name: str
    cx: float
    cy: float
    cz: float
    sx: float
    sy: float
    sz: float
    tags: tuple
    material: str = "Concrete"


def slab_pieces(spec: TowerSpec, floor: int):
    """Boxes making up one slab (with shaft + stair holes). Floor 0 (lobby) has no holes."""
    full = [Rect(0, 0, spec.building_x, spec.building_y)]
    pieces = full
    if floor > 0:
        pieces = subtract(pieces, spec.hoist_shaft)
        if spec.stair_landing_gap:
            pieces = subtract(pieces, spec.stairwell)
    top = floor * spec.floor_height
    out = []
    for i, r in enumerate(pieces):
        out.append(Box(
            name=f"Slab_F{floor:02d}_{i}",
            cx=r.x + r.w / 2, cy=r.y + r.d / 2, cz=top - spec.slab_thickness / 2,
            sx=r.w, sy=r.d, sz=spec.slab_thickness,
            tags=("Slab", f"Floor:{floor}", "Concrete"),
        ))
    return out


def shaft_walls(spec: TowerSpec):
    """Thin walls around the hoist shaft, full height. Tagged Open so they cost 0 dB (it's a cage)."""
    h = spec.num_floors * spec.floor_height + spec.floor_height
    s = spec.hoist_shaft
    t = 0.05
    return [
        Box("ShaftWall_W", s.x - t / 2, s.y + s.d / 2, h / 2, t, s.d, h, ("ShaftWall", "Open"), "Open"),
        Box("ShaftWall_E", s.x2 + t / 2, s.y + s.d / 2, h / 2, t, s.d, h, ("ShaftWall", "Open"), "Open"),
        Box("ShaftWall_S", s.x + s.w / 2, s.y - t / 2, h / 2, s.w, t, h, ("ShaftWall", "Open"), "Open"),
    ]


def floor_label(spec: TowerSpec, floor: int):
    """Position for a TextRender on the open (south, y=0) face of each slab."""
    return (spec.building_x + 1.0, -0.5, floor * spec.floor_height + 1.2)


def load_beacons(csv_path):
    with open(csv_path, newline="") as f:
        return list(csv.DictReader(f))


def beacon_position(spec: TowerSpec, row):
    """Beacons are mounted on the ceiling of their floor, 0.3 m below the slab above."""
    floor = int(row["Floor"])
    z = (floor + 1) * spec.floor_height - spec.slab_thickness - 0.3
    return (float(row["X"]), float(row["Y"]), z)


def all_boxes(spec: TowerSpec):
    boxes = []
    for f in range(0, spec.num_floors + 1):
        boxes.extend(slab_pieces(spec, f))
    boxes.extend(shaft_walls(spec))
    return boxes


if __name__ == "__main__":
    spec = TowerSpec()
    boxes = all_boxes(spec)
    print(f"{len(boxes)} boxes for {spec.num_floors + 1} slabs")
    # sanity: every slab's pieces tile the building minus holes (area check)
    for f in (0, 1, 7):
        pieces = slab_pieces(spec, f)
        area = sum(b.sx * b.sy for b in pieces)
        expect = spec.building_x * spec.building_y
        if f > 0:
            expect -= spec.hoist_shaft.w * spec.hoist_shaft.d + spec.stairwell.w * spec.stairwell.d
        assert abs(area - expect) < 1e-6, (f, area, expect)
        # no overlaps
        for i, a in enumerate(pieces):
            for b in pieces[i + 1:]:
                ra = Rect(a.cx - a.sx / 2, a.cy - a.sy / 2, a.sx, a.sy)
                rb = Rect(b.cx - b.sx / 2, b.cy - b.sy / 2, b.sx, b.sy)
                assert not ra.overlaps(rb), (f, a.name, b.name)
        print(f"  floor {f}: {len(pieces)} pieces, area {area:.2f} m² ok")
    here = os.path.dirname(os.path.abspath(__file__))
    rows = load_beacons(os.path.join(here, "..", "Data", "DT_BeaconLayout.csv"))
    print(f"{len(rows)} beacons, e.g. {rows[24]['BeaconId']} at {beacon_position(spec, rows[24])}")
