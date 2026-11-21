"""Bakes the VR gloves that OpenXWA ships.

Run with Blender, without its window:

    blender -b --python bake.py -- <inputs folder> <output folder>

The inputs are the files that fetch.sh downloads: the two skinned gloves of
Valve's SteamVR Unity plugin, their images and three hand poses. For each
hand and each pose, this writes one rigid model as a .gltf file in the
layout of a flight model (a root and one part), for aeron_gltf_cook. The
images are written once, as PNG files.

The models are in the space of the OpenXR grip pose, in metres:
  - the origin is the middle of the closed hand;
  - -Z goes through the closed fingers, from the little finger to the thumb;
  - +X is across the palm: into the palm on the right hand, out of it on the
    left hand;
  - the fingers of an open hand point along -Y.

It also prints the place of the index fingertip of each `point` model, which
is the default of `tip_m` in the game (flight.yaml, vr_hands).
"""

import json
import os
import re
import struct
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

# The pose of the plugin, and the name of the model made from it.
POSES = (("relaxed", "neutral"), ("point", "point"), ("fist", "grasp"))
# How far the middle of the closed hand is from the hand's bones, out of the
# palm, and how far it is from the wrist to the knuckles.
GRIP_OUT_OF_PALM_M = 0.03
GRIP_ALONG_PALM = 0.62


def load_pose(path, hand):
    """Reads the bone positions and rotations of one hand from a pose file."""
    text = open(path).read()
    block = text.split(hand + ":")[1]
    if hand == "leftHand":
        block = block.split("rightHand:")[0]

    def vectors(key):
        out = []
        for line in block.split(key + ":")[1].splitlines()[1:]:
            if not line.strip().startswith("- {"):
                break
            out.append([float(v) for v in re.findall(r"[xyzw]: (-?[\d.eE+-]+)", line)])
        return out

    return vectors("bonePositions"), vectors("boneRotations")


def apply_pose(arm, side, positions, rotations):
    """Poses the glove's bones. The pose files hold the right hand, in Unity's
    axes. The root, the wrist and the `aux` bones stay at rest."""
    for index, bone in enumerate(arm.data.bones):
        pose_bone = arm.pose.bones[bone.name]
        pose_bone.matrix_basis = Matrix.Identity(4)
        if index < 2 or bone.parent is None or bone.parent.name == "Root":
            continue
        rest = bone.parent.matrix_local.inverted() @ bone.matrix_local
        x, y, z, w = rotations[index]
        p = positions[index]
        right_rotation = Quaternion((w, x, -y, -z))
        right_position = Vector((-p[0], p[1], p[2]))
        if side == "right":
            rotation, position = right_rotation, right_position
        elif bone.parent.name.startswith("wrist"):
            # The bones under the wrist: mirrored in X, with a half turn
            # about X.
            rotation = Quaternion((0, 1, 0, 0)) @ right_rotation
            position = Vector((-right_position[0], right_position[1], right_position[2]))
        else:
            rotation, position = right_rotation, -right_position
        wanted = Matrix.Translation(position) @ rotation.to_matrix().to_4x4()
        pose_bone.matrix_basis = rest.inverted() @ wanted
    bpy.context.view_layer.update()


def bone_head(arm, *parts):
    """The place of a bone in the world. The bone is the one whose name has
    every part: the names differ between the two hands."""
    names = [b.name for b in arm.pose.bones if all(part in b.name for part in parts)]
    if len(names) != 1:
        raise RuntimeError("expected one bone named %s, found %s" % (parts, names))
    return arm.matrix_world @ arm.pose.bones[names[0]].head


def grip_frame(arm, side, positions):
    """The grip pose in the world, from the bones at rest and the closed fist:
    a matrix whose columns are the grip's axes, and its origin."""
    apply_pose(arm, side, *positions["fist"])
    fist_tips = sum((bone_head(arm, "finger_%s_" % f, "_end") for f in ("index", "middle", "ring", "pinky")),
                    Vector()) / 4
    for pose_bone in arm.pose.bones:
        pose_bone.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    wrist = bone_head(arm, "wrist")
    knuckles = {f: bone_head(arm, "finger_%s_0" % f) for f in ("index", "middle", "ring", "pinky")}
    middle = sum(knuckles.values(), Vector()) / 4
    fingers = (knuckles["middle"] - wrist).normalized()
    across = knuckles["index"] - knuckles["pinky"]
    across = (across - across.dot(fingers) * fingers).normalized()
    # Out of the palm: the side that the fingers close on.
    out_of_palm = fingers.cross(across).normalized()
    if out_of_palm.dot(fist_tips - middle) < 0:
        out_of_palm = -out_of_palm
    y = -fingers
    z = -across
    x = y.cross(z)
    wanted = -out_of_palm if side == "right" else out_of_palm
    if x.dot(wanted) < 0.9:
        raise RuntimeError("the %s glove's axes are not as expected" % side)
    origin = wrist + GRIP_ALONG_PALM * (middle - wrist) + GRIP_OUT_OF_PALM_M * out_of_palm
    return Matrix((x, y, z)).transposed(), origin


def baked_mesh(mesh_object, rotation, origin):
    """The posed mesh as lists of unique vertices and triangle indices, in the
    grip's space."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    mesh = bpy.data.meshes.new_from_object(mesh_object.evaluated_get(depsgraph), preserve_all_data_layers=True,
                                           depsgraph=depsgraph)
    mesh.calc_loop_triangles()
    to_grip = rotation.transposed()
    world = mesh_object.matrix_world
    normal_world = world.to_3x3().inverted().transposed()
    uv_layer = mesh.uv_layers.active.data
    # The glove is one smooth surface, so each vertex has one normal.
    vertex_normals = mesh.vertex_normals
    vertices, lookup, indices = [], {}, []
    for triangle in mesh.loop_triangles:
        for loop in triangle.loops:
            vertex = mesh.loops[loop].vertex_index
            position = to_grip @ (world @ mesh.vertices[vertex].co - origin)
            normal = (to_grip @ (normal_world @ vertex_normals[vertex].vector)).normalized()
            uv = uv_layer[loop].uv
            key = tuple(round(v, 6) for v in (*position, *normal, uv[0], 1.0 - uv[1]))
            if key not in lookup:
                lookup[key] = len(vertices)
                vertices.append(key)
            indices.append(lookup[key])
    bpy.data.meshes.remove(mesh)
    return vertices, indices


def write_gltf(folder, name, vertices, indices):
    """Writes one model: a root and one part, with the glove's two images."""
    positions = b"".join(struct.pack("<3f", *v[0:3]) for v in vertices)
    normals = b"".join(struct.pack("<3f", *v[3:6]) for v in vertices)
    uvs = b"".join(struct.pack("<2f", *v[6:8]) for v in vertices)
    index_bytes = b"".join(struct.pack("<H", i) for i in indices)
    if len(index_bytes) % 4:
        index_bytes += b"\0\0"
    chunks = (positions, normals, uvs, index_bytes)
    views, offset = [], 0
    for index, chunk in enumerate(chunks):
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(chunk),
                      "target": 34963 if index == 3 else 34962})
        offset += len(chunk)
    low = [min(v[axis] for v in vertices) for axis in range(3)]
    high = [max(v[axis] for v in vertices) for axis in range(3)]
    flight = "AERON_flight_model"
    document = {
        "asset": {"version": "2.0", "generator": "OpenXWA tools/vr_hands/bake.py",
                  "copyright": "Glove model and images: Copyright (c) Valve Corporation, BSD 3-Clause"},
        "extensionsUsed": [flight],
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [
            {"name": name, "children": [1], "extensions": {flight: {"role": "model"}}},
            {"name": "glove", "mesh": 0, "extensions": {flight: {"role": "component", "meshType": "default"}}},
        ],
        "meshes": [{"name": "glove", "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}],
        "materials": [{"name": "vr_glove",
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0,
                                                "roughnessFactor": 0.7},
                       "normalTexture": {"index": 1}}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987}],
        "images": [{"name": "vr_glove_color", "uri": "vr_glove_color.png", "mimeType": "image/png"},
                   {"name": "vr_glove_normal", "uri": "vr_glove_normal.png", "mimeType": "image/png"}],
        "textures": [{"source": 0, "sampler": 0}, {"source": 1, "sampler": 0}],
        "buffers": [{"uri": name + ".bin", "byteLength": offset}],
        "bufferViews": views,
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(vertices), "type": "VEC3", "min": low,
             "max": high},
            {"bufferView": 1, "componentType": 5126, "count": len(vertices), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(vertices), "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ],
    }
    if len(vertices) > 65535:
        raise RuntimeError("%s has too many vertices for 16-bit indices" % name)
    with open(os.path.join(folder, name + ".bin"), "wb") as out:
        out.write(b"".join(chunks))
    with open(os.path.join(folder, name + ".gltf"), "w") as out:
        json.dump(document, out, indent=1)


def save_png(source, target):
    scene = bpy.context.scene
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.view_settings.view_transform = "Standard"
    image = bpy.data.images.load(source)
    image.save_render(target, scene=scene)
    bpy.data.images.remove(image)


def main():
    inputs, output = sys.argv[sys.argv.index("--") + 1:][:2]
    os.makedirs(output, exist_ok=True)
    save_png(os.path.join(inputs, "vr_glove_color.jpg"), os.path.join(output, "vr_glove_color.png"))
    save_png(os.path.join(inputs, "vr_glove_normal.png"), os.path.join(output, "vr_glove_normal.png"))
    for side in ("left", "right"):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.wm.fbx_import(filepath=os.path.join(inputs, "vr_glove_%s_model_slim.fbx" % side))
        arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
        mesh_object = next(o for o in bpy.data.objects if o.type == "MESH")
        positions = {pose: load_pose(os.path.join(inputs, "fallback_%s.asset" % pose), side + "Hand")
                     for pose, _ in POSES}
        rotation, origin = grip_frame(arm, side, positions)
        for pose, model in POSES:
            apply_pose(arm, side, *positions[pose])
            vertices, indices = baked_mesh(mesh_object, rotation, origin)
            name = "vr_hand_%s_%s" % (side, model)
            write_gltf(output, name, vertices, indices)
            print("BAKED %s: %d vertices, %d triangles" % (name, len(vertices), len(indices) // 3))
            if model == "point":
                tip = rotation.transposed() @ (bone_head(arm, "finger_index_", "_end") - origin)
                print("TIP %s: [%.3f, %.3f, %.3f]" % (side, *tip))


main()
