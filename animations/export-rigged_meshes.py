#!/usr/bin/env python

#based on 'export-sprites.py' and 'glsprite.py' from TCHOW Rainbow; code used is released into the public domain.
#Patched for 15-466-f19 to remove non-pnct formats!
#Patched for 15-466-f20 to merge data all at once (slightly faster)

#Note: Script meant to be executed within blender 4.2.1, as per:
#blender --background --python export-meshes.py -- [...see below...]

import sys,re

args = []
for i in range(0,len(sys.argv)):
	if sys.argv[i] == '--':
		args = sys.argv[i+1:]

if len(args) < 2:
	print("\n\nUsage:\nblender --background --python export-meshes.py -- <infile.blend[:collection]> <outfile.pnct> <outfile.infl> \nExports the meshes referenced by all objects in the specified collection(s) (default: all objects) to a binary blob.\n")
	exit(1)

import bpy

infile = args[0]
collection_name = None
m = re.match(r'^(.*):([^:]+)$', infile)
if m:
	infile = m.group(1)
	collection_name = m.group(2)
outfiles = args[1:3]

for outfile in outfiles:
	assert outfile.endswith(".pnct") or outfile.endswith(".infl")

print("Will export meshes referenced from ",end="")
if collection_name:
	print("collection '" + collection_name + "'",end="")
else:
	print('master collection',end="")
print(" of '" + infile + "' to '" + ", ".join(outfiles) + "'.")

import struct

bpy.ops.wm.open_mainfile(filepath=infile)

if collection_name:
	if not collection_name in bpy.data.collections:
		print("ERROR: Collection '" + collection_name + "' does not exist in scene.")
		exit(1)
	collection = bpy.data.collections[collection_name]
else:
	collection = bpy.context.scene.collection

#meshes to write:
to_write = set()
did_collections = set()
def add_meshes(from_collection):
	global to_write
	global did_collections
	if from_collection in did_collections:
		return
	did_collections.add(from_collection)

	if from_collection.name[0] == '_':
		print("Skipping collection '" + from_collection.name + "' because its name starts with an underscore.")
		return

	for obj in from_collection.objects:
		if obj.type == 'MESH':
			if obj.data.name[0] == '_':
				print("Skipping mesh '" + obj.data.name + "' because its name starts with an underscore.")
			else:
				to_write.add(obj.data)
		if obj.instance_collection:
			add_meshes(obj.instance_collection)
	for child in from_collection.children:
		add_meshes(child)

add_meshes(collection)
#print("Added meshes from: ", did_collections)

#set all collections visible: (so that meshes can be selected for triangulation)
def set_visible(layer_collection):
	layer_collection.exclude = False
	layer_collection.hide_viewport = False
	layer_collection.collection.hide_viewport = False
	for child in layer_collection.children:
		set_visible(child)

set_visible(bpy.context.view_layer.layer_collection)

from mathutils import Matrix
def matrix_to_bytes(mat : Matrix, dim : tuple[int, int], order : str = 'c'):
	if order != 'r' and order != 'c':
		raise ValueError('Incorrect order ' + order + ". Please specify row-order 'r' or col-order 'c'.")
	
	mat_data = b''
	rows, cols = dim 
	if order == 'c':
		for j in range(cols):
			for i in range(rows):
				mat_data += struct.pack('f', mat[i][j])
	else:
		for i in range(rows):
			for j in range(cols):
				mat_data += struct.pack('f', mat[i][j])

	return mat_data

from collections import deque
def topological_sort(bones : bpy.types.ArmatureBones | bpy.types.PoseBone) -> list[bpy.types.Bone] | list[bpy.types.PoseBone]:
	bone_count = len(bones)
	roots = deque([bone for bone in bones if bone.parent is None])
	visited = {bone.name : False for bone in bones}

	sorted_bones = []
	frontier = deque()

	while len(roots) > 0:
		root : bpy.types.Bone = roots.pop()
		if not visited[root.name]:
			frontier.append(root)

		while len(frontier) > 0:
			bone : bpy.types.Bone = frontier.pop()
			visited[bone.name] = True

			for child in bone.children:
				if not visited[child.name]:
					frontier.append(child)
			sorted_bones.append(bone)

	if len(sorted_bones) != bone_count:
		raise ValueError("Topological sort failed: not all input bones are reachable from the subset.")
	
	return sorted_bones

assert(len(bpy.data.armatures) == 1 and "Cannot export rigged meshes with more than one armature in scene")
sorted_bones = topological_sort(bpy.data.armatures[0].bones)

bone_name_to_index = { sorted_bones[i].name : i for i in range(len(sorted_bones)) }

#data contains vertex, normal, color, and texture data from the meshes:
data = []

#strings contains the mesh names:
strings = b''

#index gives offsets into the data (and names) for each mesh:
index = b''

#vg_data gives flattened vertex group name indices and weights:
vg_data = b''

#vg_index gives start and end indices into vg_data per mesh
vg_index = b''

#mat0 gives mesh from parent and inverse matrices
mats = b''

vertex_count = 0

for obj in bpy.data.objects:
	if obj.data in to_write:
		to_write.remove(obj.data)
	else:
		continue

	obj.hide_select = False
	mesh = obj.data
	name = mesh.name

	print("Writing '" + name + "'...")

	if bpy.context.object:
		bpy.ops.object.mode_set(mode='OBJECT') #get out of edit mode (just in case)

	#select the object and make it the active object:
	bpy.ops.object.select_all(action='DESELECT')
	obj.select_set(True)
	bpy.context.view_layer.objects.active = obj
	bpy.ops.object.mode_set(mode='OBJECT')

	#print(obj.visible_get()) #DEBUG

	#apply all modifiers (?):
	bpy.ops.object.convert(target='MESH')

	#subdivide object's mesh into triangles:
	bpy.ops.object.mode_set(mode='EDIT')
	bpy.ops.mesh.select_all(action='SELECT')
	bpy.ops.mesh.quads_convert_to_tris(quad_method='BEAUTY', ngon_method='BEAUTY')
	bpy.ops.object.mode_set(mode='OBJECT')

	#record mesh name, start position and vertex count in the index:
	name_begin = len(strings)
	strings += bytes(name, "utf8")
	name_end = len(strings)
	index += struct.pack('I', name_begin)
	index += struct.pack('I', name_end)

	mats += matrix_to_bytes(obj.matrix_world.inverted(), (4,4))
	mats += matrix_to_bytes(obj.matrix_world, (4,4))

	index += struct.pack('I', vertex_count) #vertex_begin
	#...count will be written below

	colors = None
	if len(obj.data.color_attributes) == 0:
		print("WARNING: trying to export color data, but object '" + name + "' does not have color data; will output 0xffffffff")
	else:
		colors = obj.data.color_attributes.active_color;
		if len(obj.data.color_attributes) != 1:
			print("WARNING: object '" + name + "' has multiple vertex color layers; only exporting '" + colors.name + "'")

	uvs = None
	if len(obj.data.uv_layers) == 0:
		print("WARNING: trying to export texcoord data, but object '" + name + "' does not uv data; will output (0.0, 0.0)")
	else:
		uvs = obj.data.uv_layers.active.data
		if len(obj.data.uv_layers) != 1:
			print("WARNING: object '" + name + "' has multiple texture coordinate layers; only exporting '" + obj.data.uv_layers.active.name + "'")

	local_data = b''

	#write the mesh triangles:
	for poly in mesh.polygons:
		assert(len(poly.loop_indices) == 3)
		for i in range(0,3):
			assert(mesh.loops[poly.loop_indices[i]].vertex_index == poly.vertices[i])
			loop = mesh.loops[poly.loop_indices[i]]
			vertex : bpy.types.MeshVertex = mesh.vertices[loop.vertex_index]
			
			for x in vertex.co:
				local_data += struct.pack('f', x)
			for x in loop.normal:
				local_data += struct.pack('f', x)

			col = None
			if colors != None and colors.domain == 'POINT':
				col = colors.data[poly.vertices[i]].color
			elif colors != None and colors.domain == 'CORNER':
				col = colors.data[poly.loop_indices[i]].color
			else:
				col = (1.0, 1.0, 1.0, 1.0)
			local_data += struct.pack('BBBB', int(col[0] * 255), int(col[1] * 255), int(col[2] * 255), 255)

			if uvs != None:
				uv = uvs[poly.loop_indices[i]].uv
				local_data += struct.pack('ff', uv.x, uv.y)
			else:
				local_data += struct.pack('ff', 0, 0)

			reduced_vgs = vertex.groups
			assert(len(vertex.groups) > 0)
			if len(vertex.groups) > 4:
				reduced_vgs = sorted(vertex.groups, key=lambda vg : vg.weight, reverse=True)[:4]
			
			assert(len(reduced_vgs) > 0)

			norm = 0
			for i in range(len(reduced_vgs)):
				norm += reduced_vgs[i].weight

			bone_index = b''
			bone_weight = b''
			for i in range(4):
				if len(reduced_vgs) <= i: # pad
					bone_index += struct.pack('I', 0)
					bone_weight += struct.pack('f', 0)
					continue
				
				vg = reduced_vgs[i]
				g_idx = bone_name_to_index[obj.vertex_groups[vg.group].name]

				bone_index += struct.pack('I', g_idx)
				bone_weight += struct.pack('f', vg.weight / norm if vg.weight / norm < 1 else 1)

			vg_data += bone_index + bone_weight

		if len(local_data) > 1000:
			data.append(local_data)
			local_data = b''
	vertex_count += len(mesh.polygons) * 3

	data.append(local_data)

	index += struct.pack('I', vertex_count) #vertex_end

data = b''.join(data)

#check that code created as much data as anticipated:
assert(vertex_count * (4*3+4*3+1*4+4*2) == len(data))
assert(vertex_count * (4+4)*4 == len(vg_data))

for outfile in outfiles:
	if outfile.endswith('.pnct'):
		#write the data chunk and index chunk to an output blob:
		blob = open(outfile, 'wb')
		#first chunk: the data
		blob.write(struct.pack('4s',b'pnct')) #type
		blob.write(struct.pack('I', len(data))) #length
		blob.write(data)
		#second chunk: the strings
		blob.write(struct.pack('4s',b'str0')) #type
		blob.write(struct.pack('I', len(strings))) #length
		blob.write(strings)
		#third chunk: the index
		blob.write(struct.pack('4s',b'idx0')) #type
		blob.write(struct.pack('I', len(index))) #length
		blob.write(index)
		'''
		#fourth chunk: the parent <-> mesh matrices
		blob.write(struct.pack('4s',b'mat0')) #type
		blob.write(struct.pack('I', len(mats))) #length
		blob.write(mats)
		'''
		wrote = blob.tell()
		blob.close()
		print("Wrote " + str(wrote) + " bytes [== " + str(len(data)+8) + " bytes of data + " + str(len(strings)+8) + " bytes of strings + " + 
	  		str(len(index)+8) + " bytes of index + " + str(len(mats)+8) + " bytes of matrix_world data to '" + outfile + "'")
	elif outfile.endswith('.infl'):
		#first chunk: the flattened per vertex gr
		blob = open(outfile, 'wb')
		blob.write(struct.pack('4s',b'infl')) #type
		blob.write(struct.pack('I', len(vg_data))) #length
		blob.write(vg_data)
		#second chunk: the strings
		blob.write(struct.pack('4s',b'str0')) #type
		blob.write(struct.pack('I', len(strings))) #length
		blob.write(strings)
		#third chunk: the index
		blob.write(struct.pack('4s',b'idx0')) #type
		blob.write(struct.pack('I', len(index))) #length
		blob.write(index)
		wrote = blob.tell()
		blob.close()
		print("Wrote " + str(len(vg_data)+8) + " bytes of bone index, weight information + " + str(len(strings)+8) + " bytes of strings + " + 
	  		str(len(index)+8) + " bytes of index to '" + outfile + "'")
