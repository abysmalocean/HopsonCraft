# HopsonCraft

## Versions (As seen in the YouTube videos)

This game has gone through major rewrites and refactors overtime.

To see the source code as it was in episodes 1, 2 and 3 of "Creating Minecraft in C++/ OpenGL" videos, look at an older commit here:

https://github.com/Hopson97/HopsonCraft/tree/90a5d596d07dfe71b5dbf47ec76c0b0802ec9dfa


## Dependencies
The project requires these libraries: SFML (minimum 2.4), GLEW (OpenGL 3.3+), and OpenGL Maths Library (GLM).

Compile using compiler flags: ``-std=c++14 -O3 -s -lsfml-graphics -lsfml-audio -lsfml-system -lsfml-window``

## Contributing
An easy way to contribute is to look for the ``///@TODO`` parts in the code. please actually test before doing a pull request.

Please follow CODESTYLE.md if you plan to contribute!

## Description
Simple Minecraft clone written using C++.

## Screenshots
![Trees](http://i.imgur.com/4yvLWQE.png "Trees")

![Mountain](http://i.imgur.com/4FKz3it.png "Mountain")

![Cliff](http://i.imgur.com/PGaiYCB.png "Cliff")

## Implementation details

#### Chunks
[Chunk Section class](https://github.com/Hopson97/HopsonCraft/blob/master/Source/World/Chunk/CSection.h)

Chunks are made out of a [volume of blocks 16x16x16 in size]

They are stored in a class called "[Full_Chunk](https://github.com/Hopson97/HopsonCraft/blob/master/Source/World/Chunk/CFull_Chunk.h)", which is basically a handler for a vertical-column of these "chunk sections". There can be potentially an infinite number of sections in a full chunk.

The 16x16x16 chunks are then made into a mesh, and drawn.

#### Rendering
The camera class stores an object of a [Frustum](https://github.com/Hopson97/HopsonCraft/blob/master/Source/Maths/Frustum.cpp) type. This is used to cull out anything (For now, just chunk sections, that is not within the field of view, which increases performance a fair amount.

Each major mesh type has it's [own shader](https://github.com/Hopson97/HopsonCraft/tree/master/Source/Shaders) and [renderer](https://github.com/Hopson97/HopsonCraft/tree/master/Source/Renderer) class, each with their own unique responsabilities.

For example, the solid chunk renderer enables face-culling and disables blending before drawing, and the liquid renderer disables face-culling, enables blending, and also uploads a time variable to the "Liquid Shader" so it can have a wave effect.

#### Chunk meshes
The chunks have their own individual meshes and VAO, represented using the [Model](https://github.com/Hopson97/HopsonCraft/blob/master/Source/Model.cpp) class. As each chunk is represented using a VAO, it means they can drawn with a single draw call.

The chunk meshes are built in "[Mesh_Builder](https://github.com/Hopson97/HopsonCraft/blob/master/Source/World/Chunk/CMesh_Builder.cpp)"

1. Cycle through the blocks of a chunk in a nested for loop
2. For each block:
3. Check each block face's adjacent block.
4. If there is no solid block there, add that face to the mesh
5. Else, continue
6. But, if the block is X-Style, such as roses and tall grass, then add the faces regardless.

The above can seen by following through the "generateMesh" function.

This has to be done when the chunk is first generated and again if a block changes within the chunk. This is fairly fast, and on my machine ranges from less than 0.01ms to 5ms, depending on how many blocks it actually has to check. The reason it sometimes checks less blocks is because before actually iterating through and checking the blocks, it checks adajacent layers. If none of these layers has a transparent block, such as air, water or leaves, then it doesn't bother checking it.

The chunk meshes are actually made out [3 parts](https://github.com/Hopson97/HopsonCraft/blob/master/Source/World/Chunk/CMesh.h#L44), a solid mesh, a flora mesh, and a liquid mesh.

The solid mesh is for rendering the blocks, such as stone and grass; the flora mesh is rendering plants, such as leaves, and roses; and the liquid mesh is for rendering water blocks.

#### Collisions
THIS IS A MAJOR "TODO"

#### Block editing

This is done in the input fuction of the [Playing State]((https://github.com/Hopson97/HopsonCraft/blob/master/Source/States/SPlaying.cpp)) class.

To let the player break a block, I simply [send out a "raycast" from the player's position, a direct it to be the same as the player's look vector.

The raycast then just continues on until it hits a solid-block. Liquid and "gas" cannot be hit by this raycast.

I also [save the position of the raycast before it hits a block], This is used for block placement, and this will be placed just before.


## Credits

#### Design
Matthew Hopson, https://github.com/Hopson97/

#### Programming
Matthew Hopson, https://github.com/Hopson97/

#### Art
Matthew Hopson, https://github.com/Hopson97/


#### Special Thanks
Anton Golov, https://github.com/jesyspa

Zoltan Fazekas, https://github.com/zfazek
