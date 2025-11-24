# Exports textures from blend files, naming them after mesh names
# Usage: blender --background --python export-textures.py -- <file.blend> <out>

import sys
import os
import shutil
import re
import bpy

args = []
for i in range(len(sys.argv)):
    if sys.argv[i] == '--':
        args = sys.argv[i+1:]

if len(args) < 2:
    exit(1)

infile = args[0]
outdir = args[1]

civilian_match = re.search(r'civilian_(\d+)', infile)
civilian_suffix = "_" + civilian_match.group(1) if civilian_match else ""

bpy.ops.wm.open_mainfile(filepath=infile)
os.makedirs(outdir, exist_ok=True)

def find_texture(material):
    if not material or not material.use_nodes:
        return None
    for node in material.node_tree.nodes:
        if node.type == 'TEX_IMAGE' and node.image:
            return node.image
    return None

def save_image(image, outpath):
    if image.packed_file:
        image.save_render(outpath)
    elif image.filepath:
        src = bpy.path.abspath(image.filepath)
        shutil.copy2(src, outpath) if os.path.exists(src) else image.save_render(outpath)
    else:
        image.save_render(outpath)

exported_textures = {}
mesh_texture_map = {}

for obj in bpy.data.objects:
    if obj.type != 'MESH':
        continue
    mesh_name = obj.data.name
    if not mesh_name or mesh_name[0] == '_':
        continue
    
    materials = [mat for mat in obj.data.materials if mat] if obj.data.materials else [slot.material for slot in obj.material_slots if slot.material]
    for material in materials:
        texture = find_texture(material)
        if texture:
            if texture not in exported_textures:
                exported_textures[texture] = set()
            exported_textures[texture].add(mesh_name)
            mesh_texture_map[mesh_name] = texture
            break

for mesh_name, texture in mesh_texture_map.items():
    safe_name = mesh_name.replace("/", "_").replace("\\", "_").replace(":", "_")
    if mesh_name == "base" and civilian_suffix:
        safe_name += civilian_suffix
    outpath = os.path.join(outdir, safe_name + ".png")
    if os.path.exists(outpath):
        continue
    save_image(texture, outpath)

for image in bpy.data.images:
    if image in exported_textures or not image.name or image.size[0] == 0 or image.size[1] == 0:
        continue
    safe_name = image.name.replace("/", "_").replace("\\", "_").replace(":", "_")
    outpath = os.path.join(outdir, safe_name + ".png")
    if os.path.exists(outpath):
        continue
    save_image(image, outpath)

