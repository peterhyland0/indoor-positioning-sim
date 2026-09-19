from __future__ import annotations
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
    hoist_shaft_2: Rect | None = field(default_factory=lambda: Rect(26, 8, 3, 3))  # second hoist, east end
    stairwell: Rect = field(default_factory=lambda: Rect(1, 1, 2, 4))
    stair_landing_gap: bool = True  # keep the stair hole open on every floor (concrete shell)
    # site dressing
    column_spacing_x: float = 6.0
    column_spacing_y: float = 5.0
    column_size: float = 0.45
    drywall_floors: int = 6          # floors 1..N are at fit-out stage (partitions up); above is bare shell
    pallets_per_floor: int = 3
    seed: int = 7                    # for pallet placement only (cosmetic; not the sim RNG)


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
        if spec.hoist_shaft_2:
            pieces = subtract(pieces, spec.hoist_shaft_2)
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


def shafts(spec: TowerSpec):
    out = [spec.hoist_shaft]
    if spec.hoist_shaft_2:
        out.append(spec.hoist_shaft_2)
    return out


def shaft_walls(spec: TowerSpec):
    """Mesh cage on three sides of each hoist shaft plus a lattice mast behind it, full height. Tagged
    Open so they cost 0 dB. The south face (towards the cutaway camera) is left open so riders stay visible."""
    h = spec.num_floors * spec.floor_height + spec.floor_height
    t = 0.05
    out = []
    for i, s in enumerate(shafts(spec)):
        n = f"Shaft{i+1}"
        out += [
            Box(f"{n}Wall_W", s.x - t / 2, s.y + s.d / 2, h / 2, t, s.d, h, ("ShaftWall", "Open"), "Open"),
            Box(f"{n}Wall_E", s.x2 + t / 2, s.y + s.d / 2, h / 2, t, s.d, h, ("ShaftWall", "Open"), "Open"),
            Box(f"{n}Wall_N", s.x + s.w / 2, s.y2 + t / 2, h / 2, s.w, t, h, ("ShaftWall", "Open"), "Open"),
            # lattice mast: a square-section column behind the shaft with cross-bracing suggested by 4 thin verticals
            Box(f"{n}Mast", s.x + s.w / 2, s.y2 + 0.6, (h + 4) / 2, 0.6, 0.6, h + 4, ("Mast", "Open"), "Steel"),
        ]
        for k in range(0, int(h // 1.5)):
            z = k * 1.5 + 0.75
            out.append(Box(f"{n}MastTie_{k}", s.x + s.w / 2, s.y2 + 0.3, z, 0.12, 0.6, 0.12, ("Mast", "Open"), "Steel"))
    return out


def columns(spec: TowerSpec):
    """Structural columns on a grid, floor 0 slab up to the roof. Tagged Concrete: they attenuate radio."""
    h = spec.num_floors * spec.floor_height
    out = []
    holes = shafts(spec) + [spec.stairwell]
    cx = spec.column_spacing_x
    x = cx / 2
    while x < spec.building_x:
        y = spec.column_spacing_y / 2
        while y < spec.building_y:
            r = Rect(x - spec.column_size / 2, y - spec.column_size / 2, spec.column_size, spec.column_size)
            if not any(r.overlaps(Rect(hh.x - 0.5, hh.y - 0.5, hh.w + 1, hh.d + 1)) for hh in holes):
                out.append(Box(f"Col_{len(out)}", x, y, h / 2, spec.column_size, spec.column_size, h, ("Column", "Concrete"), "Concrete"))
            y += spec.column_spacing_y
        x += cx
    return out


def edge_rails(spec: TowerSpec):
    """Edge protection along the open slab edges on every floor above the lobby: posts + two rails.
    Tagged Open (no RF effect). South edge left clear in the middle third for the cutaway view."""
    out = []
    post_h = 1.1
    for f in range(1, spec.num_floors + 1):
        z0 = f * spec.floor_height
        edges = [
            ("N", 0, spec.building_y, spec.building_x, 0, spec.building_x),
            ("W", 0, 0, 0, 1, spec.building_y),
            ("E", spec.building_x, 0, spec.building_x, 1, spec.building_y),
            ("S", 0, 0, spec.building_x, 0, spec.building_x),
        ]
        for name, x0, y0, x1, axis, length in edges:
            spans = [(0, length)]
            if name == "S":
                spans = [(0, length / 3), (2 * length / 3, length)]
            for a, b in spans:
                for rail_z in (0.55, 1.05):
                    if axis == 0:
                        out.append(Box(f"Rail_F{f:02d}_{name}_{int(a)}", (a + b) / 2, y0, z0 + rail_z, b - a, 0.05, 0.05, ("Rail", "Open"), "Steel"))
                    else:
                        out.append(Box(f"Rail_F{f:02d}_{name}_{int(a)}", x0, (a + b) / 2, z0 + rail_z, 0.05, b - a, 0.05, ("Rail", "Open"), "Steel"))
                p = a
                while p <= b:
                    if axis == 0:
                        out.append(Box(f"Post_F{f:02d}_{name}_{int(p)}", p, y0, z0 + post_h / 2, 0.06, 0.06, post_h, ("Rail", "Open"), "Steel"))
                    else:
                        out.append(Box(f"Post_F{f:02d}_{name}_{int(p)}", x0, p, z0 + post_h / 2, 0.06, 0.06, post_h, ("Rail", "Open"), "Steel"))
                    p += 2.5
    return out


def drywall(spec: TowerSpec):
    """Fit-out partitions on floors 1..drywall_floors: a corridor wall along the core and a few room walls.
    Tagged Drywall: 4 dB per crossing. Openings (doorways) left every few metres."""
    out = []
    wall_h = spec.floor_height - spec.slab_thickness - 0.05
    t = 0.12
    for f in range(1, min(spec.drywall_floors, spec.num_floors) + 1):
        z = f * spec.floor_height + wall_h / 2
        # corridor wall y = 6.5 and y = 13.5, with 1.2 m doorways every 6 m
        for y in (6.5, 13.5):
            x = 0.5
            while x < spec.building_x - 0.5:
                seg = min(4.8, spec.building_x - 0.5 - x)
                out.append(Box(f"Drywall_F{f:02d}_y{y}_{int(x)}", x + seg / 2, y, z, seg, t, wall_h, ("Partition", "Drywall"), "Drywall"))
                x += seg + 1.2
        # room walls off the north corridor
        for x in (5.0, 10.5, 20.5, 25.0):
            out.append(Box(f"Drywall_F{f:02d}_x{x}", x, 16.75, z, t, 6.5, wall_h, ("Partition", "Drywall"), "Drywall"))
    return out


def pallets(spec: TowerSpec):
    """Material stacks scattered on each floor (cosmetic clutter, tagged Open)."""
    import random
    rng = random.Random(spec.seed)
    out = []
    holes = shafts(spec) + [spec.stairwell]
    for f in range(0, spec.num_floors + 1):
        z0 = f * spec.floor_height
        for k in range(spec.pallets_per_floor):
            for _ in range(20):
                x = rng.uniform(2, spec.building_x - 2)
                y = rng.uniform(2, spec.building_y - 2)
                r = Rect(x - 0.6, y - 0.5, 1.2, 1.0)
                if not any(r.overlaps(Rect(hh.x - 1, hh.y - 1, hh.w + 2, hh.d + 2)) for hh in holes):
                    break
            hgt = rng.uniform(0.4, 1.3)
            out.append(Box(f"Pallet_F{f:02d}_{k}", x, y, z0 + hgt / 2, 1.2, 1.0, hgt, ("Clutter", "Open"), rng.choice(["Timber", "Plasterboard", "Blocks"])))
    return out


def site(spec: TowerSpec):
    """Everything outside the building footprint: ground, perimeter fence, site hut, tower crane."""
    out = []
    g = 40.0
    out.append(Box("Ground", spec.building_x / 2, spec.building_y / 2, -0.15, spec.building_x + 2 * g, spec.building_y + 2 * g, 0.3, ("Ground", "Open"), "Gravel"))
    # hoarding fence around the site (2.4 m high), gate gap on the south side near the entrance
    fx0, fy0 = -g + 6, -g + 6
    fx1, fy1 = spec.building_x + g - 6, spec.building_y + g - 6
    out.append(Box("Fence_N", (fx0 + fx1) / 2, fy1, 1.2, fx1 - fx0, 0.05, 2.4, ("Fence", "Open"), "Hoarding"))
    out.append(Box("Fence_W", fx0, (fy0 + fy1) / 2, 1.2, 0.05, fy1 - fy0, 2.4, ("Fence", "Open"), "Hoarding"))
    out.append(Box("Fence_E", fx1, (fy0 + fy1) / 2, 1.2, 0.05, fy1 - fy0, 2.4, ("Fence", "Open"), "Hoarding"))
    out.append(Box("Fence_S1", (fx0 + 2) / 2, fy0, 1.2, 2 - fx0, 0.05, 2.4, ("Fence", "Open"), "Hoarding"))
    out.append(Box("Fence_S2", (8 + fx1) / 2, fy0, 1.2, fx1 - 8, 0.05, 2.4, ("Fence", "Open"), "Hoarding"))
    # site hut + welfare cabin outside the south-west corner
    out.append(Box("SiteHut", -8, 4, 1.4, 6, 2.5, 2.8, ("Hut", "Open"), "Cabin"))
    out.append(Box("SiteHut2", -8, 8, 1.4, 6, 2.5, 2.8, ("Hut", "Open"), "Cabin"))
    # tower crane behind the building (north side) so it reads over the roofline from the cutaway camera
    h = spec.num_floors * spec.floor_height + 14
    cx, cy = spec.building_x * 0.72, spec.building_y + 9
    out.append(Box("CraneMast", cx, cy, h / 2, 1.8, 1.8, h, ("Crane", "Open"), "Steel"))
    out.append(Box("CraneJib", cx - 16, cy, h + 1, 46, 1.2, 1.4, ("Crane", "Open"), "Steel"))
    out.append(Box("CraneCounter", cx + 9, cy, h + 1, 12, 1.2, 1.4, ("Crane", "Open"), "Steel"))
    out.append(Box("CraneCab", cx, cy - 1.6, h - 1, 2.4, 1.6, 2.2, ("Crane", "Open"), "Cabin"))
    # hook block hanging from the jib
    out.append(Box("CraneHook", cx - 30, cy, h - 12, 0.6, 0.6, 1.2, ("Crane", "Open"), "Steel"))
    out.append(Box("CraneCable", cx - 30, cy, h - 5.5, 0.05, 0.05, 11, ("Crane", "Open"), "Steel"))
    # skip and a couple of parked vans
    out.append(Box("Skip", spec.building_x + 4, 2, 0.7, 3.6, 1.8, 1.4, ("Clutter", "Open"), "Steel"))
    out.append(Box("Van1", -6, -6, 1.0, 5.2, 2.0, 2.0, ("Clutter", "Open"), "Cabin"))
    out.append(Box("Van2", 2, -6, 1.0, 5.2, 2.0, 2.0, ("Clutter", "Open"), "Cabin"))
    return out


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


def all_boxes(spec: TowerSpec, dressing: bool = True):
    boxes = []
    for f in range(0, spec.num_floors + 1):
        boxes.extend(slab_pieces(spec, f))
    boxes.extend(shaft_walls(spec))
    if dressing:
        boxes.extend(columns(spec))
        boxes.extend(edge_rails(spec))
        boxes.extend(drywall(spec))
        boxes.extend(pallets(spec))
        boxes.extend(site(spec))
    return boxes


if __name__ == "__main__":
    spec = TowerSpec()
    boxes = all_boxes(spec)
    kinds = {}
    for b in boxes:
        kinds[b.tags[0]] = kinds.get(b.tags[0], 0) + 1
    print(f"{len(boxes)} boxes for {spec.num_floors + 1} slabs: {kinds}")
    # sanity: every slab's pieces tile the building minus holes (area check)
    for f in (0, 1, 7):
        pieces = slab_pieces(spec, f)
        area = sum(b.sx * b.sy for b in pieces)
        expect = spec.building_x * spec.building_y
        if f > 0:
            expect -= sum(hh.w * hh.d for hh in shafts(spec)) + spec.stairwell.w * spec.stairwell.d
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
