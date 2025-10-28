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

This game was built with [NEST](NEST.md).

