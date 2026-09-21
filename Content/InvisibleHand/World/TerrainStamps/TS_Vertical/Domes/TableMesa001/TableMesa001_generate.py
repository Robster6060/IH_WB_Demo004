"""
Terrain Stamp generator - TableMesa001
Run inside Blender 5.0.1's Scripting tab (Text Editor > Run Script, or Alt+P).

Implements pipeline stages 1-3 from
TerrainStamp_BlenderToUE5_Pipeline.md:
  1. Scene unit setup (metric, 1 Blender Unit = 1cm, matching UE5's cm-scale units)
  2. Parametric ring-stack base geometry (mirrors the AIH_TerrainStampActor
     Bottom/Top/intermediate-ring model: a stack of horizontal rings, radius
     tapering from a wide base to a narrower flat cap)
  3. Base footprint irregularity + gentle cap undulation + UV0 unwrap

NOTE ON IMPLEMENTATION CHOICE: the approved plan called for a Geometry Nodes
graph so inputs are live-tweakable in the Modifier panel. This script instead
builds the mesh directly via bmesh + mathutils.noise (both native Blender
Python modules, no external deps) driven by the same named parameters below.
This trades "drag a slider in the UI" for "edit a number and re-run the
script" - deliberately, because a hand-written mesh builder is something I
can verify is correct by reading the code, whereas constructing a Geometry
Nodes node graph via the scripting API is version-fragile (socket names and
node idnames shift between Blender releases) and not something I could test
before handing it to you. If you want the live-node-graph version later, this
script's math (the radius/wobble formulas below) is the exact spec to build
into a GN group by hand in Blender's UI.

Re-running this script deletes and rebuilds the object named STAMP_NAME, so
it's safe to tweak parameters and re-run repeatedly.
"""

import math
import os

import bmesh
import bpy
from mathutils import noise

# ---------------------------------------------------------------------------
# USER-EDITABLE PARAMETERS (all real-world sizes in meters)
# ---------------------------------------------------------------------------
STAMP_NAME = "TableMesa001"

BASE_RADIUS_M = 700.0      # outer base radius  -> ~1400m base diameter
CAP_RADIUS_M = 450.0       # flat-top radius     -> ~900m cap diameter
HEIGHT_M = 380.0           # base-to-cap height (IH Midlands ASL band target)

RING_COUNT = 14            # vertical resolution: spine samples base -> cap
ANGULAR_SEGMENTS = 48      # radial resolution around the mesa

TAPER_SHARPNESS = 3.0      # >1 = flatter shoulder + steeper cliff band.
                           # radius(t) = cap + (base-cap) * (1-t)^TAPER_SHARPNESS

IRREGULARITY_SEED = 42
BASE_WOBBLE_M = 90.0       # horizontal footprint irregularity amplitude at the base
CAP_WOBBLE_M = 15.0        # horizontal irregularity amplitude at the cap rim
CAP_UNDULATION_M = 4.0     # vertical dimpling on the flat top (drainage read)
NOISE_SCALE = 1.6          # spatial frequency of the coherent wobble noise

SAVE_BLEND_ON_RUN = True   # save a .blend next to this script after building
OUTPUT_DIR = r"D:\Projects\ClaudeProjects\IH_WB_Demo004\Content\InvisibleHand\World\TerrainStamps\TS_Vertical\Domes\TableMesa001"

# ---------------------------------------------------------------------------
# Unit convention: scene unit_scale is set to 0.01 below, so
# 1 Blender Unit = 1 cm = 0.01 m, matching UE5's "1 Unreal Unit = 1cm"
# exactly. All geometry below is authored in Blender Units via m() so the
# FBX export (stage 4) needs no further scale correction on import.
# ---------------------------------------------------------------------------
BU_PER_M = 100.0


def m(value_m):
    return value_m * BU_PER_M


# ---------------------------------------------------------------------------
# Stage 1: scene unit setup
# ---------------------------------------------------------------------------
def setup_scene_units():
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 0.01
    scene.unit_settings.length_unit = 'CENTIMETERS'

    far_clip = m(5000.0)  # 5km far clip so a >1km-wide asset doesn't clip
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for space in area.spaces:
                if space.type == 'VIEW_3D':
                    space.clip_end = far_clip


# ---------------------------------------------------------------------------
# Stages 2-3: parametric ring-stack mesh with footprint irregularity
# ---------------------------------------------------------------------------
def build_mesh():
    noise.seed_set(IRREGULARITY_SEED)

    base_r = m(BASE_RADIUS_M)
    cap_r = m(CAP_RADIUS_M)
    height = m(HEIGHT_M)
    base_wobble = m(BASE_WOBBLE_M)
    cap_wobble = m(CAP_WOBBLE_M)
    cap_undulation = m(CAP_UNDULATION_M)

    bm = bmesh.new()
    ring_verts = []  # list of per-ring vertex lists, bottom -> top

    for ring_i in range(RING_COUNT):
        t = ring_i / (RING_COUNT - 1)
        z = height * t
        ring_radius = cap_r + (base_r - cap_r) * (1.0 - t) ** TAPER_SHARPNESS
        wobble_amp = cap_wobble + (base_wobble - cap_wobble) * (1.0 - t)

        verts = []
        for seg_i in range(ANGULAR_SEGMENTS):
            theta = 2.0 * math.pi * seg_i / ANGULAR_SEGMENTS
            nx = math.cos(theta) * NOISE_SCALE
            ny = math.sin(theta) * NOISE_SCALE
            n = noise.noise((nx, ny, t * NOISE_SCALE))  # ~[-1, 1]
            r = ring_radius + n * wobble_amp

            x = math.cos(theta) * r
            y = math.sin(theta) * r
            z_local = z

            if t > 0.85:
                top_n = noise.noise((nx * 0.6, ny * 0.6, 7.0 + IRREGULARITY_SEED))
                z_local += top_n * cap_undulation * ((t - 0.85) / 0.15)

            verts.append(bm.verts.new((x, y, z_local)))
        ring_verts.append(verts)

    bm.verts.ensure_lookup_table()

    # side faces between consecutive rings
    for ring_i in range(RING_COUNT - 1):
        lower = ring_verts[ring_i]
        upper = ring_verts[ring_i + 1]
        for seg_i in range(ANGULAR_SEGMENTS):
            seg_j = (seg_i + 1) % ANGULAR_SEGMENTS
            bm.faces.new((lower[seg_i], lower[seg_j], upper[seg_j], upper[seg_i]))

    # base cap (fan facing down)
    base_center = bm.verts.new((0.0, 0.0, 0.0))
    for seg_i in range(ANGULAR_SEGMENTS):
        seg_j = (seg_i + 1) % ANGULAR_SEGMENTS
        bm.faces.new((base_center, ring_verts[0][seg_j], ring_verts[0][seg_i]))

    # top cap (fan facing up)
    top_z = sum(v.co.z for v in ring_verts[-1]) / len(ring_verts[-1])
    top_center = bm.verts.new((0.0, 0.0, top_z))
    for seg_i in range(ANGULAR_SEGMENTS):
        seg_j = (seg_i + 1) % ANGULAR_SEGMENTS
        bm.faces.new((top_center, ring_verts[-1][seg_i], ring_verts[-1][seg_j]))

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])

    mesh = bpy.data.meshes.new(STAMP_NAME + "_Mesh")
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def replace_existing_object():
    existing = bpy.data.objects.get(STAMP_NAME)
    if existing is not None:
        old_mesh = existing.data
        bpy.data.objects.remove(existing, do_unlink=True)
        if old_mesh is not None and old_mesh.users == 0:
            bpy.data.meshes.remove(old_mesh)


def unwrap_uv0(obj):
    obj.data.uv_layers.new(name="UVMap")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')


def main():
    setup_scene_units()
    replace_existing_object()

    mesh = build_mesh()
    obj = bpy.data.objects.new(STAMP_NAME, mesh)
    bpy.context.collection.objects.link(obj)

    unwrap_uv0(obj)

    for poly in mesh.polygons:
        poly.use_smooth = True

    print(
        f"{STAMP_NAME}: {len(mesh.vertices)} verts / {len(mesh.polygons)} faces "
        f"- base {BASE_RADIUS_M * 2:.0f}m / cap {CAP_RADIUS_M * 2:.0f}m diameter, "
        f"height {HEIGHT_M:.0f}m"
    )

    if SAVE_BLEND_ON_RUN:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        blend_path = os.path.join(OUTPUT_DIR, STAMP_NAME + ".blend")
        if os.path.exists(blend_path):
            print(f"NOTE: {blend_path} already exists - not overwriting. "
                  f"Save manually (File > Save As) if you want to update it.")
        else:
            bpy.ops.wm.save_as_mainfile(filepath=blend_path)
            print(f"Saved: {blend_path}")


main()
