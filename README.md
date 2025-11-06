# (TODO: your game's title)

Author: (TODO: your name)

Design: (TODO: In two sentences or fewer, describe what is new and interesting about your game.)

Text Drawing: (TODO: how does the text drawing in this game work? Is text precomputed? Rendered at runtime? What files or utilities are involved?)

Choices: (TODO: how does the game store choices and narrative? How are they authored? Anything nifty you want to point out?)

Screen Shot:

![Screen Shot](screenshot.png)

How To Play:

(TODO: describe the controls and (if needed) goals/strategy.)

```
/Applications/Blender.app/Contents/MacOS/Blender -y --background --python scenes/export-meshes.py -- scenes/zoo_nolink.blend:Main dist/zoo_nolink.pnct && /Applications/Blender.app/Contents/MacOS/Blender -y --background --python scenes/export-scene.py -- scenes/zoo_nolink.blend:Main dist/zoo_nolink.scene

node Maekfile.js && dist/game
```

Sources: (TODO: list a source URL for any assets you did not create yourself. Make sure you have a license for the asset.)

Low Poly Evergreen Tree Short 3: https://www.blenderkit.com/get-blenderkit/8a94fa91-95a8-4997-8ec3-392fa148da9e/
Low Poly Evergreen Tree Tall 2: https://www.blenderkit.com/get-blenderkit/ad5bf1e4-854c-4a2c-b33d-49f308a2b882/
Cobblestone road: https://www.blenderkit.com/get-blenderkit/c71aae1e-fa64-4454-85e0-4f76b49b9a7b/
Street lamp: https://www.blenderkit.com/get-blenderkit/58ed8a12-f81f-4f24-ba5b-de43e1d6baf8/
Man model for separate body parts: https://www.fab.com/listings/65434ad8-bfd3-466d-bc4f-6319ec3366f8
Fence Segment: https://www.blenderkit.com/get-blenderkit/0470ca8c-0719-4404-a4ef-47794852264e/
deer dead body: https://sketchfab.com/3d-models/deer-dead-body-832225752ec84608b9fa7643b764d79c
Dead rat: https://sketchfab.com/3d-models/dead-rat-f0690b4bf5a940a385556b43dda4dad7
Vintage Street Lamp: https://www.blenderkit.com/get-blenderkit/d585a3d2-6677-4de6-a3e4-81b89486253a/
Park Bench: https://www.blenderkit.com/get-blenderkit/0e35aec5-21cf-4e03-883e-008756e7d8bf/
No Parking Sign Board: https://www.blenderkit.com/get-blenderkit/40bd752c-0163-4482-8298-0fe9e0aec7a4/
Iron gate: https://www.blenderkit.com/get-blenderkit/cc604c67-711c-4645-bdd6-4d8655dffdc1/
Brick and Wroute Iron Fence Segment: https://www.blenderkit.com/get-blenderkit/7865ad3b-ae62-403e-80d4-99ea9fa49789/

This game was built with [NEST](NEST.md).

