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
	print("\n\nUsage:\nblender --background --python export-rig.py -- <infile.blend[:collection]> <outfile.skel> <outfile.anim>\nExports the skeleton and corresponding animations referenced by the single armature in the specified collection(s) (default: all objects) to a binary blob. Can omit at most one outfile.\n")
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
	assert outfile.endswith(".skel") or outfile.endswith(".anim")

print("Will export armature and animations referenced from ",end="")
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

to_write = set()
did_collections = set()
def add_armatures(from_collection):
	global to_write
	global did_collections
	if from_collection in did_collections:
		return
	did_collections.add(from_collection)
	
	if from_collection.name[0] == '_':
		print("Skipping collection '" + from_collection.name + "' because its name starts with an underscore.")
		return

	for obj in collection.objects:
		if obj.type == "ARMATURE":
			if obj.name[0] == '_':
				print("Skipping armature '" + obj.data.name + "' because its name starts with an underscore.")
			elif len(to_write) > 0:
				raise ValueError('More than one armature detected. For the purpose of data extraction, please make sure each .blend has a single armature.')
			else:
				to_write.add(obj.data)
		if obj.instance_collection:
			add_armatures(obj.instance_collection)

	for child in from_collection.children:
		add_armatures(child)

def add_actions():
	global to_write
	global did_collections
	for action in bpy.data.actions:
		if action.name[0] == '_':
			print("Skipping action '" + action.name + "' because its name starts with an underscore.")
		else:
			to_write.add(action)

add_armatures(collection)
assert(len(to_write) == 1)
add_actions()

from mathutils import Matrix, Quaternion, Vector
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

#skeleton to write:
def write_skeleton(obj : bpy.types.Object, outfile):
	assert(obj.type == "ARMATURE")
	armature = obj.data

	#data contains bone information from the armature:
	data = []

	#strings contains the armature names:
	strings = b''

	#index0 gives offsets into the data (and names) for each armature:
	index0 = b''

	# index1 gives offset into the names for each bone:
	index1 = b''

	# matrices in bytes of world_from_skeleton matrices
	mats = b''

	name = armature.name

	print("-- Writing skeleton '" + name + "'...")

	# PROCESS BONE INFORMATION

	# enforce rest position
	armature.pose_position = 'REST'

	# prepare dictionary of bone to index in bones list
	bones = topological_sort(armature.bones)
	bone_count = len(bones)
	bone_indices = {bones[i].name : i for i in range(len(bones))}
	bone_index = 0

	#record mesh name, start position and bone count in the index:
	name_begin = len(strings)
	strings += bytes(name, "utf8")
	name_end = len(strings)
	index0 += struct.pack('I', name_begin)
	index0 += struct.pack('I', name_end)
	index0 += struct.pack('I', bone_index) #bone_begin

	local_data = b''

	mats += matrix_to_bytes(obj.matrix_world.inverted(), (4,4))
	mats += matrix_to_bytes(obj.matrix_world, (4,4))

	#write the bones:
	for i in range(bone_count):
		bone = bones[i]

		parent = bone_indices[bone.parent.name] if bone.parent else bone_indices[bone.name] # self if no parent
		assert(parent <= i)

		index1 += struct.pack('I', len(strings))
		strings += bytes(bone.name, "utf8")
		index1 += struct.pack('I', len(strings))

		# pack bone parent index
		local_data += struct.pack('I', parent)
		
		local_data += matrix_to_bytes(bone.matrix_local.inverted(), (4,4))
		local_data += matrix_to_bytes(bone.matrix_local, (4,4))

		if len(local_data) > 1000:
			data.append(local_data)
			local_data = b''
		
	
	data.append(local_data)

	bone_index += bone_count
	index0 += struct.pack('I', bone_index) #vertex_end

	# put world_from_skeleton data first, since 1 matrix per armature
	data = b''.join(data)

	#check that code created as much data as anticipated:
	assert(2*(4*4*4) == len(mats))
	assert(bone_index*(4+(4*4)*4+(4*4)*4) == len(data))

	#write the data chunk and index chunk to an output blob:
	blob = open(outfile, 'wb')
	#first chunk: the data
	blob.write(struct.pack('4s',b'skel')) #type
	blob.write(struct.pack('I', len(data))) #length
	blob.write(data)
	#second chunk: world from skeleton transformations
	blob.write(struct.pack('4s',b'mat0')) #type
	blob.write(struct.pack('I', len(mats))) #length
	blob.write(mats)
	#third chunk: the strings
	blob.write(struct.pack('4s',b'str0')) #type
	blob.write(struct.pack('I', len(strings))) #length
	blob.write(strings)
	#fourth chunk: armature index
	blob.write(struct.pack('4s',b'idx0')) #type
	blob.write(struct.pack('I', len(index0))) #length
	blob.write(index0)
	#fifth chunk: bone index
	blob.write(struct.pack('4s',b'idx1')) #type
	blob.write(struct.pack('I', len(index1))) #length
	blob.write(index1)
	wrote = blob.tell()
	blob.close()

	print("Wrote " + str(wrote) + " bytes [== " + str(len(data)+8) + " bytes of data + " + str(len(mats)+8) +
		" bytes of world from skeleton matrices + " + str(len(strings)+8) + " bytes of strings + " + str(len(index0)+8) + " bytes of armature index + " + str(len(index1)+8) + " bytes of bone index] to '" + outfile + "'")

#animations corresponding to skeleton
def write_animations(obj : bpy.types.Object, outfile):
	assert(obj.type == "ARMATURE")
	armature = obj.data

	#data contains N keyframes, holding storing world_to_local 4x4 matrices for each bone at each keyframe:
	data = []

	#strings contains the action names:
	strings = b''

	#actor gives number of actors of each action:
	actor_ct = b''

	#index0 gives offsets into the data (and names) for each action:
	index0 = b''

	#index1 gives offsets into the names for each actor:
	index1 = b''

	#times gives the time, start index, and end index of each keyframe 
	keyframes = b''

	# set armature to pose position
	bpy.context.view_layer.objects.active = obj
	bpy.ops.object.mode_set(mode='POSE')
	armature.pose_position = 'POSE'

	pose_bones = topological_sort(obj.pose.bones)
	bones = topological_sort(obj.data.bones)
	actor_count = len(pose_bones)
	
	keyframe_total = 0
	for action in bpy.data.actions:
		if action in to_write:
			to_write.remove(action)
		else:
			continue

		name = action.name

		print("-- Writing action '" + name + "'...")

		# assign the action to the armature
		obj.animation_data.action = action
		obj.animation_data.action_slot = action.slots.active
		bpy.context.scene.frame_set(int(action.frame_range[0]))
		bpy.context.view_layer.update()

		
		keyframe_index = 0

		#record action name, start position and  count in the index:
		name_begin = len(strings)
		strings += bytes(name, "utf8")
		name_end = len(strings)
		actor_ct += struct.pack('I', actor_count)
		index0 += struct.pack('I', name_begin)
		index0 += struct.pack('I', name_end)
		index0 += struct.pack('I', keyframe_total) #bone_begin

		local_data = b''
		
		fps : float = bpy.context.scene.render.fps / bpy.context.scene.render.fps_base
		
		# get the frames of each keyframe
		frames = sorted({key.co.x for fcurve in action.fcurves for key in fcurve.keyframe_points})
		
		for i in frames:
			# update the view to this frame: https://blender.stackexchange.com/questions/132403/pose-bones-matrix-does-not-update-after-frame-change-via-script
			bpy.context.scene.frame_set(int(i))
			bpy.context.view_layer.update()
			for j in range(actor_count):
				t, r, s = (pose_bones[j].matrix_basis).decompose()
				r.normalize()
				local_data += struct.pack('fff', *t)
				local_data += struct.pack('ffff', r.x, r.y, r.z, r.w)
				local_data += struct.pack('fff', *s)

				if len(local_data) > 1000:
					data.append(local_data)
					local_data = b''
				
			keyframes += struct.pack('f', i / fps)
			keyframes += struct.pack('I', keyframe_index)
			keyframes += struct.pack('I', actor_count)

			keyframe_index += actor_count

		for j in range(actor_count):
			index1 += struct.pack('I', len(strings))
			strings += bytes(pose_bones[j].name, "utf8")
			index1 += struct.pack('I', len(strings))

		data.append(local_data)
		keyframe_total += keyframe_index
		index0 += struct.pack('I', keyframe_total) #action_end

	data = b''.join(data)

	# reset rest position
	armature.pose_position = 'REST'
	bpy.ops.object.mode_set(mode='OBJECT')

	#check that code created as much data as anticipated:
	assert(keyframe_total*(4*3+4*4+4*3) == len(data))

	#write the data chunk and index chunk to an output blob:
	blob = open(outfile, 'wb')
	#first chunk: the data
	blob.write(struct.pack('4s',b'anim')) #type
	blob.write(struct.pack('I', len(data))) #length
	blob.write(data)
	#second chunk: the strings
	blob.write(struct.pack('4s',b'str0')) #type
	blob.write(struct.pack('I', len(strings))) #length
	blob.write(strings)
	#fourth chunk: actor counts of all actions
	blob.write(struct.pack('4s',b'act0')) #type
	blob.write(struct.pack('I', len(actor_ct))) #length
	blob.write(actor_ct)
	#fourth chunk: times for each keyframe
	blob.write(struct.pack('4s',b'fps0')) #type
	blob.write(struct.pack('I', 4)) #length
	blob.write(struct.pack('I', bpy.context.scene.render.fps))
	#third chunk: action index
	blob.write(struct.pack('4s',b'idx0')) #type
	blob.write(struct.pack('I', len(index0))) #length
	blob.write(index0)
	#third chunk: actor name index
	blob.write(struct.pack('4s',b'idx1')) #type
	blob.write(struct.pack('I', len(index1))) #length
	blob.write(index1)
	#fifth chunk: times for each keyframe
	blob.write(struct.pack('4s',b'keys')) #type
	blob.write(struct.pack('I', len(keyframes))) #length
	blob.write(keyframes)

	wrote = blob.tell()
	blob.close()

	print("Wrote " + str(wrote) + " bytes [== " + str(len(data)+8) + " bytes of data + " + str(len(strings)+8) + " bytes of strings + " 
	    + str(len(index0)+8) + " bytes of action extent + " + str(len(index1)+8) + " bytes of actor names + " + str(len(actor_ct)+8) + " bytes of actor counts per action + "
		+ str(len(keyframes)+8) + " bytes for keyframes + 12 bytes for fps)] to '" + outfile + "'")

# process
def set_visible(layer_collection):
	layer_collection.exclude = False
	layer_collection.hide_viewport = False
	layer_collection.collection.hide_viewport = False
	for child in layer_collection.children:
		set_visible(child)

set_visible(bpy.context.view_layer.layer_collection)

arm_obj = None
for obj in bpy.context.scene.objects:
	if obj.type == "ARMATURE":
		if obj.data in to_write:
			to_write.remove(obj.data)
		else:
			continue
		
		obj.hide_select = False

		if bpy.context.object:
			bpy.ops.object.mode_set(mode='OBJECT') #get out of edit mode (just in case)

		#select the object and make it the active object:
		bpy.ops.object.select_all(action='DESELECT')
		obj.select_set(True)
		bpy.context.view_layer.objects.active = obj
		bpy.ops.object.mode_set(mode='OBJECT')
		
		#print(obj.visible_get()) #DEBUG

		arm_obj = obj
		break

assert(arm_obj and arm_obj.type == "ARMATURE")
for outfile in outfiles:
	if outfile.endswith(".skel"):
		write_skeleton(arm_obj, outfile)
	elif outfile.endswith(".anim"):
		write_animations(arm_obj, outfile)