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
/Applications/Blender.app/Contents/MacOS/Blender -y --background --python scenes/export-meshes.py -- scenes/zoo.blend:Main dist/zoo.pnct
/Applications/Blender.app/Contents/MacOS/Blender -y --background --python scenes/export-scene.py -- scenes/zoo.blend:Main dist/zoo.scene

node Maekfile.js && dist/game
```

Sources: (TODO: list a source URL for any assets you did not create yourself. Make sure you have a license for the asset.)

This game was built with [NEST](NEST.md).

