# How to rig a mesh
All data remains decoupled; user couples what they want with wrappers. An example can be run by renaming <code>example_rig_PlayMode.*pp</code> to <code>PlayMode.*pp</code>.

## 1. Run export-rigged_meshes.py and export-rig.py using the following

- ```blender.exe --background --python export-rigged_meshes.py -- <.blend> <.pnct> <.infl>```
- ```blender.exe --background --python export-rig.py -- <.blend> <.skel> <.anim>```

The two output files are optional--if you only wish to extract mesh information, you can omit .infl, and vice-versa (same with export-rig.py).

Blender guarantees same bone ordering as long as .blend is not modified. If you update .blend, it is safest extract all outputs.

- ```.pnct``` - unchanged from standard mesh exporting
- ```.infl``` - the 4 highest weighted vertex groups per vertex, in the same vertex order returned in .pnct. 

Vertex weights should not be used without corresponding meshes, so .infl only included vertex group information and indexing information is obtained from .pnct.

- ```.skel``` - the skeleton in rest position
- ```.anim``` - animations used by the skeleton in the corresponding skeleton file

Blender maintains that animations and skeletons are independent objects, i.e. an animation defined on skeleton is not restricted to that skeleton. An animation works so long as the skeleton bones are named the same as the actors in the animation.

## 2. Create buffers for the data

Fill in data paths as necessary. The following is a minimal buffer loading setup for rigging.
```cpp
#include "Mesh.hpp"
#include "Skeleton.hpp"
#include "RiggedMesh.hpp"
#include "Animation.hpp"

Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("test.pnct"));
	return ret;
});

Load< BoneInfluenceBuffer > bone_infls(LoadTagDefault, []() -> BoneInfluenceBuffer const * {
	BoneInfluenceBuffer const *ret = new BoneInfluenceBuffer(data_path("test.infl"));
	return ret;
});

Load< SkeletonBuffer > skeletons(LoadTagDefault, []() -> SkeletonBuffer const * {
	SkeletonBuffer const *ret = new SkeletonBuffer(data_path("../animations/test.skel"));
	return ret;
});

Load< AnimationBuffer< Skeleton::BoneTransform > > animations(LoadTagDefault, []() -> AnimationBuffer< Skeleton::BoneTransform > const * {
	AnimationBuffer< Skeleton::BoneTransform > const *ret = new AnimationBuffer< Skeleton::BoneTransform >(data_path("../animations/test.anim"));
	return ret;
});
```

## 3. Create animation graph.

If a rig is being applied to a mesh, the intention is that it will be animated. User is expected to maintain an animation graph per rigged mesh. An animation graph requires a user-defined interpolation function of type
```cpp
std::function< T(const T&, const T&) >
```
although a simple non-interp solution is to just return a copy of the original. 

Animation graphs can animate any number of actors, as long as they are all of the same type.

```cpp
interp = auto [](Skeleton::BoneTransform &a, Skeleton::BoneTransfrom &b) {
    return Skeleton::BoneTransfrom(a);
}
AnimationGraph < Skeleton::BoneTransform > graph(interp);
```

- If you do not have predefined animations one can create their own animation. An animation clip, like an animation graph, can animate any number of actors, as long as they are all of the same type. An animation clip requires a name, fps, and whether the clip loops or not, and can be later updated.

```cpp
Animation < T >(std::string name, uint32_t fps, bool loop);
```

One can then begin to add keyframes to the animation using

```cpp
void add_keyframes(const std::vector< T > &data, const std::vector< float > &times, const std::vector< std::string > &property_names={});
```

The first call to <code>add_keyframes</code> requires the user to specify <code>property_names</code>, after which it is optional (but if filled, must match the initial). All defined-keyframes must list data for each defined property/actor for each specified time in <code>times</code>. Over all calls to <code>add_keyframes</code>, concatenation of <code>times</code> should be strictly increasing. Each call to ```add_keyframes``` appends the current input to the existing animation data. Data is laid out such that, if there are $n$ actors, the first $n$ entries into ```data``` is the information for the corresponding actor at ``times[0]``, the next $n$ entries is information for actors at ```times[1]```, and so on.

- Otherwise, if you have a predefined <code>AnimationBuffer</code> for the rig, you can <code>lookup</code> the animation by name from Blender.

### Registering animation clips
Animation clips can be registered via

```cpp
graph.add_state(const Animation< T > &animation);
```
and transitions can be defined by
```cpp
graph.add_transition(std::string state, Transition transtition);
```

Where <code>state</code> is the initial state and <code>Transition</code> is a pair of condition for transition and name of state to transition to; explicitly with the following type

```cpp
typedef std::pair < std::function< bool (AnimationGraph< T > &) >, std::string > Transition;
```

The condition function of a transition takes as input a reference to the graph it is registered to, if the user wishes to transition conditionally on playback time, for example.

If you wish to sample interpolated information on your own, outside of the skeleton animations (for waypoint system/color transitions or such), the following line returns interpolation data for all the properties specified, in the same order.

```cpp
auto data = graph.sample();
```

Explicilty call to <code>sample</code> is not necessary for skinning, as it is handled by <code>RiggedMesh</code>.

## 4. Create rigged mesh

A rigged mesh, as implemented, does not explicitly use the mesh reference it is given, it simply includes it to couple the mesh and rig for user-convenience. With all the above defined, a mesh can be rigged as follows

```cpp
// constructor: RiggedMesh(GLuint vbo_vert, GLuint vbo_bone, const Mesh &mesh, const Skeleton &skel, AnimationGraph< T > *graph);

const Mesh &mesh = meshes->lookup("Mesh");
const Skeleton &skel = skeletons->lookup("Skeleton");

RiggedMesh rigged_mesh = RiggedMesh(meshes->buffer, bone_infls->buffer, mesh, skel, &graph);
```

This <code>RiggedMesh</code> object takes the place of a <code>Mesh</code> object that would have been used when creating a <code>Drawable</code>. The <code>Drawable</code> is populated using the defined skinning program and the same process as always.

```cpp
#include "SkinningProgram.hpp"

drawable.pipeline = skinning_program_pipeline;
drawable.pipeline.vao = rigged_mesh->make_vao_for_program(skinning_program->program);
drawable.pipeline.type = rigged_mesh->mesh.type;
drawable.pipeline.start = rigged_mesh->mesh.start;
drawable.pipeline.count = rigged_mesh->mesh.count;
```

Then, in the update loop, remember to update the AnimationGraph and the RiggedMesh.

```cpp
void PlayMode::update(float elapsed) {
    ...
    graph.update(elapsed);
    rigged_mesh.update(elapsed);
}
```
