"""Author actual material-prop geometry in centimetres; no external Python packages required.

Exports editable OBJ meshes with UVs, normals, named material slots and a geometry manifest.
Run with Python 3, then import_materials_unreal.py inside Unreal Editor.
"""
from pathlib import Path
import json
import math
import random

ROOT = Path(__file__).resolve().parent
TAU = math.tau


def add(a, b): return tuple(x+y for x, y in zip(a, b))
def sub(a, b): return tuple(x-y for x, y in zip(a, b))
def mul(a, s): return tuple(x*s for x in a)
def dot(a, b): return sum(x*y for x, y in zip(a, b))
def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def unit(a): return mul(a, 1/max(1.e-12, math.sqrt(dot(a, a))))


class Mesh:
    def __init__(self, name):
        self.name = name
        self.vertices, self.uvs, self.normals, self.faces = [], [], [], []

    def surface(self, points, faces, material, smooth=False, uvs=None):
        offset = len(self.vertices)
        triangles = [(f[0], f[i], f[i+1]) for f in faces for i in range(1, len(f)-1)]
        if smooth:
            normals = [(0., 0., 0.) for _ in points]
            for a, b, c in triangles:
                normal = cross(sub(points[b], points[a]), sub(points[c], points[a]))
                for i in (a, b, c): normals[i] = add(normals[i], normal)
            self.vertices.extend(points)
            self.normals.extend(unit(n) for n in normals)
            self.uvs.extend(uvs or [(p[0]/10, p[1]/10) for p in points])
            self.faces.extend((tuple(i+offset for i in tri), material) for tri in triangles)
        else:
            for tri in triangles:
                normal = unit(cross(sub(points[tri[1]], points[tri[0]]), sub(points[tri[2]], points[tri[0]])))
                if dot(normal, normal) < .1: continue
                ids = []
                for i in tri:
                    ids.append(len(self.vertices))
                    self.vertices.append(points[i]); self.normals.append(normal)
                    p = points[i]
                    axes = sorted(range(3), key=lambda a: abs(normal[a]))[:2]
                    self.uvs.append(uvs[i] if uvs else (p[axes[0]]/10, p[axes[1]]/10))
                self.faces.append((tuple(ids), material))

    def export(self):
        minimum = [min(p[i] for p in self.vertices) for i in range(3)]
        maximum = [max(p[i] for p in self.vertices) for i in range(3)]
        pivot = ((minimum[0]+maximum[0])/2, (minimum[1]+maximum[1])/2, minimum[2])
        self.vertices = [sub(p, pivot) for p in self.vertices]
        folder = ROOT/'Meshes'; folder.mkdir(parents=True, exist_ok=True)
        path = folder/('SM_Item_'+self.name+'.obj')
        lines = ['# Authored material prop, centimetres, Z up.', 'mtllib CraftingMaterials.mtl', 'o SM_Item_'+self.name]
        lines += ['v %.7f %.7f %.7f' % p for p in self.vertices]
        lines += ['vt %.7f %.7f' % p for p in self.uvs]
        lines += ['vn %.7f %.7f %.7f' % p for p in self.normals]
        previous = None
        for ids, material in self.faces:
            if material != previous: lines.append('usemtl '+material); previous = material
            lines.append('f '+' '.join('%d/%d/%d' % (i+1, i+1, i+1) for i in ids))
        path.write_text('\n'.join(lines)+'\n', encoding='utf-8')
        return dict(name=self.name, triangles=len(self.faces), vertices=len(self.vertices),
                    materials=sorted(set(m for _, m in self.faces)),
                    bounds_cm=dict(min=[min(p[i] for p in self.vertices) for i in range(3)], max=[max(p[i] for p in self.vertices) for i in range(3)]))


def transform(p, center=(0, 0, 0), angle=0, axis='Z'):
    x, y, z = p
    if axis == 'X': x, y, z = z, x, y
    elif axis == 'Y': x, y, z = y, z, x
    c, s = math.cos(angle), math.sin(angle)
    return (center[0]+x*c-y*s, center[1]+x*s+y*c, center[2]+z)


def outline(w, d, clip):
    x, y = w/2, d/2; clip = min(clip, x*.8, y*.8)
    return [(-x+clip, -y), (x-clip, -y), (x, -y+clip), (x, y-clip), (x-clip, y), (-x+clip, y), (-x, y-clip), (-x, -y+clip)]


def box(mesh, center, size, material, bevel=.12, angle=0, clip=None):
    w, d, h = size; bevel = min(bevel, h*.3, w*.15, d*.15)
    clip = clip if clip is not None else max(bevel, .02)
    rings = []
    for z, inset in [(-h/2, bevel), (-h/2+bevel, 0), (h/2-bevel, 0), (h/2, bevel)]:
        rings.append([transform((x, y, z), center, angle) for x, y in outline(w-2*inset, d-2*inset, max(.01, clip-inset*.5))])
    n = 8
    mesh.surface(rings[0], [tuple(reversed(range(n)))], material)
    mesh.surface(rings[-1], [tuple(range(n))], material)
    for lo, hi in zip(rings, rings[1:]):
        mesh.surface(lo+hi, [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)], material)


def revolve(mesh, center, profile, material, segments=48, axis='Z', capped=True):
    rings = [[transform((r*math.cos(TAU*i/segments), r*math.sin(TAU*i/segments), z), center, axis=axis) for i in range(segments)] for r, z in profile]
    n = segments
    for lo, hi in zip(rings, rings[1:]):
        mesh.surface(lo+hi, [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)], material, True,
                     [(i/n, j) for j in range(2) for i in range(n)])
    if capped:
        mesh.surface(rings[0], [tuple(reversed(range(n)))], material)
        mesh.surface(rings[-1], [tuple(range(n))], material)


def ring(mesh, center, inner, outer, z0, z1, material, segments=48, axis='Z'):
    rings = [[transform((r*math.cos(TAU*i/segments), r*math.sin(TAU*i/segments), z), center, axis=axis) for i in range(segments)] for r, z in [(outer, z0), (outer, z1), (inner, z0), (inner, z1)]]
    n = segments
    for left, right, reverse, smooth in [(0, 1, False, True), (2, 3, True, True), (1, 3, False, False), (0, 2, True, False)]:
        faces = [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)]
        if reverse: faces = [tuple(reversed(f)) for f in faces]
        mesh.surface(rings[left]+rings[right], faces, material, smooth)


def tube(mesh, points, radius, material, sides=6):
    rings = []
    for i, p in enumerate(points):
        tangent = unit(sub(points[min(i+1, len(points)-1)], points[max(i-1, 0)]))
        reference = (0, 0, 1) if abs(tangent[2]) < .9 else (0, 1, 0)
        n = unit(cross(reference, tangent)); b = cross(tangent, n)
        rings.append([add(p, mul(add(mul(n, math.cos(TAU*j/sides)), mul(b, math.sin(TAU*j/sides))), radius)) for j in range(sides)])
    points_out = [p for r in rings for p in r]
    faces = [(i*sides+j, i*sides+(j+1)%sides, (i+1)*sides+(j+1)%sides, (i+1)*sides+j) for i in range(len(rings)-1) for j in range(sides)]
    mesh.surface(points_out, faces, material, True)
    mesh.surface(rings[0], [tuple(reversed(range(sides)))], material)
    mesh.surface(rings[-1], [tuple(range(sides))], material)


def triangulate(points):
    """Bowyer-Watson triangulation of a planar point set, used for the drilled plates."""
    points = list(points); count = len(points)
    extent = max(max(abs(x), abs(y)) for x, y in points)*20
    points.extend([(-extent, -extent), (extent, -extent), (0, extent)])
    tris = [(count, count+1, count+2)]
    def orientation(a, b, c): return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    def inside(tri, p):
        a, b, c = [sub(points[i], p) for i in tri]
        det = dot(a, a)*(b[0]*c[1]-c[0]*b[1])-dot(b, b)*(a[0]*c[1]-c[0]*a[1])+dot(c, c)*(a[0]*b[1]-b[0]*a[1])
        return det > 1.e-9
    for i in range(count):
        bad = [t for t in tris if inside(t, points[i])]
        edges = {}
        for t in bad:
            for a, b in [(t[0], t[1]), (t[1], t[2]), (t[2], t[0])]:
                key = tuple(sorted((a, b))); edges[key] = edges.get(key, 0)+1
        tris = [t for t in tris if t not in bad]
        for (a, b), number in edges.items():
            if number != 1: continue
            if orientation(points[a], points[b], points[i]) < 0: a, b = b, a
            tris.append((a, b, i))
    return [t for t in tris if max(t) < count]


def drilled_plate(mesh, center, w, d, thickness, angle):
    boundary = outline(w, d, 1.25)
    holes = [(x, y) for x in [-w/2+2.2, w/2-2.2] for y in [-d/2+2.2, d/2-2.2]]
    radius = .7; n = 24
    points = boundary+[(cx+radius*math.cos(TAU*i/n), cy+radius*math.sin(TAU*i/n)) for cx, cy in holes for i in range(n)]
    def keep(t):
        centroid = tuple(sum(points[i][a] for i in t)/3 for a in range(2))
        return all((centroid[0]-cx)**2+(centroid[1]-cy)**2 > radius**2*.98 for cx, cy in holes)
    faces = [t for t in triangulate(points) if keep(t)]
    for z, reverse in [(0, True), (thickness, False)]:
        mesh.surface([transform((x, y, z), center, angle) for x, y in points], [tuple(reversed(t)) if reverse else t for t in faces], 'Steel')
    for loop, reverse in [(boundary, False)]+[([(cx+radius*math.cos(TAU*i/n), cy+radius*math.sin(TAU*i/n)) for i in range(n)], True) for cx, cy in holes]:
        side = [transform((x, y, z), center, angle) for z in [0, thickness] for x, y in loop]; k = len(loop)
        side_faces = [(i, (i+1)%k, (i+1)%k+k, i+k) for i in range(k)]
        mesh.surface(side, [tuple(reversed(f)) if reverse else f for f in side_faces], 'Steel', reverse)


def steel_plate():
    m = Mesh('SteelPlate')
    drilled_plate(m, (-.35, -.1, .12), 22, 18, 1.15, -.035)
    drilled_plate(m, (.3, .1, 1.52), 22, 18, 1.15, .025)
    box(m, (0, 0, 2.83), (1.95, 19, .19), 'BlueBand', .04)
    box(m, (0, 0, .1), (1.95, 19, .19), 'BlueBand', .04)
    for y in [-9.45, 9.45]: box(m, (0, y, 1.45), (1.95, .19, 2.9), 'BlueBand', .04)
    box(m, (0, -1.0, 3.02), (2.35, 1.5, .35), 'Steel', .1)
    for y in [-1.65, -.35]: box(m, (0, y, 3.22), (1.7, .08, .05), 'DarkSteel', .01)
    return m


def fiber_roll():
    m = Mesh('ReinforcedFiber'); n = 360
    start = -.6*math.pi; end = start+4.6*TAU
    for inner, outer, material in [(0, .30, 'Aramid'), (-.30, 0, 'GraphiteFabric')]:
        points = []
        for i in range(n+1):
            angle = start+(end-start)*i/n; r = 1.75+5.3*i/n
            for x, dr in [(-12.5, inner), (12.5, inner), (-12.5, outer), (12.5, outer)]:
                points.append((x, (r+dr)*math.cos(angle), 7.55+(r+dr)*math.sin(angle)))
        for a, b, reverse in [(0, 1, True), (2, 3, False), (0, 2, False), (1, 3, True)]:
            faces = [(4*i+a, 4*(i+1)+a, 4*(i+1)+b, 4*i+b) for i in range(n)]
            if reverse: faces = [tuple(reversed(f)) for f in faces]
            m.surface(points, faces, material, a in [0, 2] and b == a+1)
        m.surface(points[:4], [(0, 1, 3, 2)], material)
        m.surface(points[-4:], [(0, 2, 3, 1)], material)
    ring(m, (0, 0, 7.55), 1.35, 1.52, -12.65, 12.65, 'Cardboard', axis='X')
    for x in [-7.1, 7.1]:
        ring(m, (0, 0, 7.55), 7.35, 7.56, x-1.1, x+1.1, 'Webbing', segments=64, axis='X')
        # Rounded rectangular buckle, crossbar, and visible stitch dashes.
        for bx in [x-1.45, x+1.45]: box(m, (bx, -7.55, 8.3), (.5, .65, 3.5), 'Rubber', .16)
        for bz in [6.8, 8.3, 9.8]: box(m, (x, -7.55, bz), (3.1, .65, .45), 'Rubber', .15)
        for angle in [TAU*i/32 for i in range(32)]:
            for sx in [x-.85, x+.85]:
                p = (sx, 7.59*math.cos(angle), 7.55+7.59*math.sin(angle))
                tube(m, [add(p, (0, -.1*math.sin(angle), .1*math.cos(angle))), add(p, (0, .1*math.sin(angle), -.1*math.cos(angle)))], .026, 'Thread', sides=4)
    return m


def gear(mesh, center, radius, teeth, material, height=1.):
    n = teeth*4; inner = radius*.17
    profile = [radius*(.86 if i%4 in [0, 3] else 1.0) for i in range(n)]
    for z, reverse in [(0, True), (height, False)]:
        points = [add(center, (r*math.cos(TAU*i/n), r*math.sin(TAU*i/n), z)) for rlist in [profile, [inner]*n] for i, r in enumerate(rlist)]
        faces = [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)]
        mesh.surface(points, [tuple(reversed(f)) if reverse else f for f in faces], material)
    for radii, reverse in [(profile, False), ([inner]*n, True)]:
        points = [add(center, (r*math.cos(TAU*i/n), r*math.sin(TAU*i/n), z)) for z in [0, height] for i, r in enumerate(radii)]
        faces = [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)]
        mesh.surface(points, [tuple(reversed(f)) if reverse else f for f in faces], material, reverse)
    ring(mesh, center, inner, radius*.32, height, height+.25, 'Steel')


def screw(mesh, x, y, z, radius=.5):
    revolve(mesh, (x, y, z), [(radius*.86, 0), (radius, .1), (radius, .25), (radius*.8, .32)], 'Steel', segments=16)
    for a in [0, math.pi/2]: box(mesh, (x, y, z+.325), (radius*1.05, .11, .025), 'DarkSteel', .01, angle=a)


def precision_parts():
    m = Mesh('PrecisionParts')
    box(m, (0, 0, .75), (27, 12, 1.5), 'DarkSteel', .2, clip=1.2)
    for y in [-5.25, 5.25]: box(m, (0, y, 2.15), (24, 1.1, 2), 'Gunmetal', .16)
    for x in [-11.9, 11.9]:
        for y in [-4.3, 4.3]: screw(m, x, y, 1.55, .6)
    gear(m, (-6.4, 0, 1.7), 4.65, 22, 'Brass', 1.15)
    gear(m, (.8, 0, 1.7), 2.85, 14, 'Steel', 1.15)
    for x in [5.8, 10.6]: box(m, (x, 0, 3.25), (.7, 6.2, 3.4), 'Gunmetal', .15)
    revolve(m, (8.2, 0, 3.35), [(1.4, -2.7), (1.4, 2.7)], 'DarkSteel', axis='Y')
    coil = [(8.2+1.55*math.cos(TAU*8*i/192), -2.6+5.2*i/192, 3.35+1.55*math.sin(TAU*8*i/192)) for i in range(193)]
    tube(m, coil, .115, 'Copper', sides=6)
    for y in [-2.1, 2.1]:
        box(m, (12.3, y, 2.6), (2.2, 1.5, 2.3), 'Rubber', .15)
        revolve(m, (0, y, 2.7), [(.4, 12.5), (.4, 16.2), (.32, 16.4)], 'Steel', segments=20, axis='X')
        ring(m, (0, y, 2.7), .4, .58, 13, 13.35, 'Brass', segments=20, axis='X')
    return m


def catalyst():
    m = Mesh('MutationCatalyst')
    revolve(m, (0, 0, 0), [(4.4, 0), (5, .4), (5, 1.5), (4.4, 2.2)], 'Gunmetal', segments=64)
    ring(m, (0, 0, 0), 3.8, 4.5, 2.2, 2.65, 'Brass', segments=64)
    revolve(m, (0, 0, 0), [(4.18, 2.6), (4.18, 15.3)], 'Glass', segments=64, capped=False)
    revolve(m, (0, 0, 0), [(4.4, 15.3), (4.85, 15.8), (4.85, 17.7), (4.25, 18.2)], 'Gunmetal', segments=64)
    ring(m, (0, 0, 0), 3.8, 4.5, 14.85, 15.3, 'Brass', segments=64)
    revolve(m, (0, 0, 0), [(4.87, 16.8), (4.87, 17.15)], 'SafetyYellow', segments=64, capped=False)
    ring(m, (0, 0, 0), 3.62, 3.72, 18.2, 18.24, 'DarkSteel', segments=64)
    for phi in [math.pi*.25+i*math.pi/2 for i in range(4)]:
        x, y = 4.25*math.cos(phi), 4.25*math.sin(phi)
        box(m, (x, y, 8.7), (.75, .65, 13.7), 'Gunmetal', .14, angle=phi)
        for z in [2.0, 15.3]: box(m, (x, y, z), (1.25, .8, 1.7), 'Gunmetal', .13, angle=phi)
        screw(m, 3.65*math.cos(phi), 3.65*math.sin(phi), 18.1, .4)
    rng = random.Random(17)
    for i in range(11):
        radius = rng.uniform(.45, .85); h = rng.uniform(3.5, 7.8)
        x, y = rng.uniform(-1.6, 1.6), rng.uniform(-1.6, 1.6); z = rng.uniform(3.0, 6.0)
        n = 5; points = [(x+radius*math.cos(TAU*j/n), y+radius*math.sin(TAU*j/n), z) for j in range(n)]
        points += [(x+radius*.7*math.cos(TAU*j/n), y+radius*.7*math.sin(TAU*j/n), z+h*.72) for j in range(n)]
        points.append((x+rng.uniform(-.8, .8), y+rng.uniform(-.7, .7), z+h))
        for j in range(n):
            m.surface(points, [(j, (j+1)%n, (j+1)%n+n, j+n), (j+n, (j+1)%n+n, 2*n)], 'Crystal' if j%3 else 'CrystalDark')
        m.surface(points[:n], [tuple(reversed(range(n)))], 'CrystalDark')
    return m


if __name__ == '__main__':
    manifests = [factory().export() for factory in [steel_plate, fiber_roll, precision_parts, catalyst]]
    materials = sorted(set(m for item in manifests for m in item['materials']))
    (ROOT/'Meshes/CraftingMaterials.mtl').write_text('\n\n'.join('newmtl '+m+'\nKd 0.35 0.35 0.35' for m in materials)+'\n', encoding='utf-8')
    (ROOT/'mesh_manifest.json').write_text(json.dumps(manifests, indent=2), encoding='utf-8')
    print(json.dumps(manifests, indent=2))
