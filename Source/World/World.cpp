#include "World.h"

#include <iostream>
#include <SFML/Graphics.hpp>

#include "../Renderer/RMaster.h"
#include "../Player/Player.h"
#include "../Camera.h"
#include "../Maths/Position_Conversion.h"

World::World(const World_Settings& worldSettings, const Camera& camera)
:   m_worldSettings (worldSettings)
,   m_chunks        (*this)
,   m_pCamera       (&camera)
{
    //Just loads a few chunks in the centre. This causes the world to open instantly.
    int32_t centre = getWorldSettings().worldSize / 2;
    for (int32_t x = -1 ; x <= 1; x++)
    {
        for (int32_t z = -1; z <= 1; z++)
        {
            m_chunks.addChunk({centre + x, centre + z}, true);
        }
    }

    m_threads.emplace_back([&]()
    {
        while (m_isRunning)
        {
            buildMeshes(*m_pCamera);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
}

World::~World()
{
    m_isRunning = false;
    for (auto& thread : m_threads)
    {
        thread.join();
    }
}

void World::updateChunks(const Player& player)
{
    if (!m_newBlocks.empty())
    {
        regenerateChunks();
    }

    for (auto itr = m_chunks.getChunks().begin(); itr != m_chunks.getChunks().end(); itr++)
    {
        Chunk::Full_Chunk& chunk = itr->second;
        chunk.tick();
    }

    if (!m_deleteChunks.empty())
    {
        m_deleteMutex.lock();
        for (auto& chunkLocation : m_deleteChunks)
        {
            m_chunks.deleteChunk(chunkLocation);
        }
        m_deleteChunks.clear();
        m_deleteMutex.unlock();
    }
}


void World::checkPlayerBounds(Player& player)
{
    if (player.position.x < 0.2)
    {
        player.position.x = 0.2;
    }
    if (player.position.z < 0.2)
    {
        player.position.z = 0.2;
    }

    if (m_worldSettings.isInfiniteTerrain)
    {
        return;
    }
    else
    {
        if (player.position.x + 0.2 > m_worldSettings.worldSize * CHUNK_SIZE - 0.2 )
        {
            player.position.x = m_worldSettings.worldSize * CHUNK_SIZE - 0.3;
        }
        if (player.position.z + 0.2 > m_worldSettings.worldSize * CHUNK_SIZE - 0.2 )
        {
            player.position.z = m_worldSettings.worldSize * CHUNK_SIZE - 0.3;
        }
    }
}

void World::qSetBlock(const Vector3& position, CBlock block)
{
    setBlock(position, block);
}

void World::setBlock(const Vector3& position, CBlock block)
{
    m_newBlocks.emplace_back(block, position);

    for (int y = -1; y <= 1; y++)
    for (int x = -1; x <= 1; x++)
    for (int z = -1; z <= 1; z++)
    {
        getBlock(position + Vector3{x, y, z}).getType().trigger(*this, {int(position.x + x),
                                                                        int(position.y + y),
                                                                        int(position.z + z)});
    }
}

CBlock World::getBlock(const Vector3& position)
{
    auto chunkPos = Maths::worldToChunkPos(position);

    if (m_chunks.existsAt(chunkPos))
    {
        auto blockPosition = Maths::worldToBlockPos(position);
        return m_chunks.get(chunkPos)->qGetBlock(blockPosition);
    }
    else
    {
        return Block::ID::Air;
    }
}

uint32_t World::getHeightAt(const Vector3& worldPosition)
{
    auto    chunkPosition   = Maths::worldToChunkPos(worldPosition);
    auto    blockPosition   = Maths::blockToSmallBlockPos(Maths::worldToBlockPos(worldPosition));

    const auto& chunk = m_chunks.get(chunkPosition);

    return chunk->getHeightAt(blockPosition.x, blockPosition.z);
}


void World::regenerateChunks()
{
    std::unordered_map<Chunk::Chunklet_Position, Chunk::Section*> chunksToUpdate;

    //Duh, inserts a chunk into the map..
    auto insertChunk = [&](const Chunk::Chunklet_Position& chunkPosition,
                                 Chunk::Section* chunk)
    {
        const auto& chunkFull = m_chunks.get({chunkPosition.x, chunkPosition.z});
        if(!chunkFull)
            return;

        while (!chunk)
        {
            chunkFull->addSection();
            chunk = chunkFull->getSection(chunkPosition.y);
        }
        chunksToUpdate.insert(std::make_pair(chunkPosition, chunk));
    };

    //Lambda checks if blocks are being set on chunk edges, and if so, adds the adjacent chunk to the update batch
    auto checkForBatchAdd = [&](int8_t position,
                                const Chunk::Chunklet_Position& chunkPosition,
                                const Vector3& direction)
    {
        if (position == 0)
        {
            Chunk::Chunklet_Position newChunkPosition(chunkPosition.x - direction.x,
                                                      chunkPosition.y - direction.y,
                                                      chunkPosition.z - direction.z);

            insertChunk(newChunkPosition, m_chunks.get(newChunkPosition));
        }
        else if (position == CHUNK_SIZE - 1)
        {
            Chunk::Chunklet_Position newChunkPosition(chunkPosition.x + direction.x,
                                                      chunkPosition.y + direction.y,
                                                      chunkPosition.z + direction.z);

            insertChunk(newChunkPosition, m_chunks.get(newChunkPosition));
        }
    };

    //Lambda's end above, function technically begins here
    for (New_Block& newBlock : m_newBlocks)
    {
        //Get respective positions and objects
        auto    chunkPosition   = Maths::worldToChunkletPos(newBlock.position);
        auto    blockPosition   = Maths::blockToSmallBlockPos(Maths::worldToBlockPos(newBlock.position));
        const auto& chunkFull   = m_chunks.get({chunkPosition.x, chunkPosition.z});

        Chunk::Section* chunk = nullptr;

        //Adds sections onto a chunk until it reaches the height of the block
        while (!chunk)
        {
            chunkFull->addSection();
            chunk = chunkFull->getSection(chunkPosition.y);
        }


        //Set block
        chunk->qSetBlock(blockPosition, newBlock.block);

        //Insert chunks into a regeneration map
        insertChunk(chunkPosition, chunk);
        //Check neighbours of this chunk, and then add those into regeneration queue thing
        checkForBatchAdd(blockPosition.x, chunkPosition, {1, 0, 0});
        checkForBatchAdd(blockPosition.y, chunkPosition, {0, 1, 0});
        checkForBatchAdd(blockPosition.z, chunkPosition, {0, 0, 1});
    }

    //Regenerate chunks
    for (auto& chunk : chunksToUpdate)
    {
        Chunk::Section* sect = chunk.second;
        sect->makeMesh();
        sect->bufferMesh();
    }

    m_newBlocks.clear();
}

//Generates meshes for the chunks.
//It does this in a sort of radius starting from the middle of the world
//This is ran concurrently alongside the main render/ update thread
void World::buildMeshes(const Camera& camera)
{
    if (m_loadingDistance == ((m_worldSettings.worldSize / 2) + 1))
    {
        m_loadingDistance = 1;
    }

    m_cameraPosition = Maths::worldToChunkPos(camera.position);

    int32_t minDisX = 0;
    int32_t maxDisX = 0;

    int32_t minDisZ = 0;
    int32_t maxDisZ = 0;

    bool isMeshMade = false;

    if (m_worldSettings.isInfiniteTerrain)
    {
        minDisX = m_cameraPosition.x - m_loadingDistance;
        maxDisX = m_cameraPosition.x + m_loadingDistance;

        minDisZ = m_cameraPosition.y - m_loadingDistance;
        maxDisZ = m_cameraPosition.y + m_loadingDistance;
    }
    else
    {
        int32_t minDis = m_worldSettings.worldSize / 2 - m_loadingDistance;
        int32_t maxDis = m_worldSettings.worldSize / 2 + m_loadingDistance;

        minDisX = minDis;
        maxDisX = maxDis;

        minDisZ = minDis;
        maxDisZ = maxDis;

    }

    m_deleteMutex.lock();
    for (int32_t x = minDisX; x < maxDisX; x++)
    {
        for (int32_t z = minDisZ; z < maxDisZ; z++)
        {
            Chunk::Position position(x, z);
            if(!m_chunks.existsAt(position))
            {
                m_chunks.addChunk(position, true);
            }

            Chunk::Full_Chunk* chunk = m_chunks.get({x, z});

            if(chunk->tryGen())
            {
                isMeshMade = true;
                break;
            }
        }
        if (isMeshMade)
        {
            break;
        }
    }
    if (!isMeshMade)
    {
        m_loadingDistance++;
    }
    if (m_worldSettings.isInfiniteTerrain)
    {
        int minX = m_cameraPosition.x - m_worldSettings.worldSize / 2 - 1;
        int maxX = m_cameraPosition.x + m_worldSettings.worldSize / 2 + 1;
        int minZ = m_cameraPosition.y - m_worldSettings.worldSize / 2 - 1;
        int maxZ = m_cameraPosition.y + m_worldSettings.worldSize / 2 + 1;

        for(auto& chunk : m_chunks.getChunks())
        {
            Chunk::Full_Chunk& c = chunk.second;
            auto location = c.getPosition();

            //Check bounds
            if (location.x <= minX ||
                location.x >= maxX ||
                location.y <= minZ ||
                location.y >= maxZ)
            {
                //If the chunk is outside of the bounds of the render distance, then add the position of it into a delete vector
                if (!c.hasDeleteFlag)
                {
                    c.hasDeleteFlag = true;
                    m_deleteChunks.push_back(location);
                }
            }
        }
    }

    m_deleteMutex.unlock();
}

void World::draw(Renderer::Master& renderer, const Camera& camera)
{
    m_facesDrawn = 0;
    for (auto& chunk : m_chunks.getChunks())
    {
        if (chunk.second.hasGeneratedBlockData)
            m_facesDrawn += chunk.second.draw(renderer, camera);
    }
}

void World::drawWorld(Renderer::Master& renderer, const Camera& camera)
{
    draw(renderer, camera);
}

const World_Settings& World::getWorldSettings() const
{
    return m_worldSettings;
}

AABB World::getBlockAABB(const Block::Position& position)
{
    AABB blockAABB({1, 1, 1});
    blockAABB.update({position.x, position.y, position.z});
    return blockAABB;
}
