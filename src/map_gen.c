#include "global.h"
#include "blend_palette.h"
#include "event_object_movement.h"
#include "field_camera.h"
#include "fldeff_poison.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "map_gen.h"
#include "main.h"
#include "overworld.h"
#include "random.h"
#include "sound.h"
#include "constants/bg_event_constants.h"
#include "constants/event_objects.h"
#include "constants/map_types.h"
#include "constants/maps.h"
#include "constants/event_object_movement_constants.h"
#include "constants/songs.h"
#include "constants/weather.h"
#include "constants/region_map_sections.h"
#include "constants/vars.h"

extern struct Tileset gTileset_General;
extern struct Tileset gTileset_Building;
extern struct Tileset gTileset_PetalburgGym;
extern struct Tileset gTileset_MossdeepGym;

extern u8 S_HackGym_Statue[];
extern u8 S_HackGym_Leader[];
extern u8 S_HackGym_Regenerate[];
extern u8 S_HackGym_Trainer1[];
extern u8 S_HackGym_Trainer2[];

#define TOP_BORDER_PADDING 1
#define MIN_MAP_GEN_WIDTH 8
#define MIN_MAP_GEN_HEIGHT 8
#define MAX_MAP_GEN_WIDTH 20
#define MAX_MAP_GEN_HEIGHT 20
#define MAX_MAP_TILES (MAX_MAP_GEN_WIDTH * (MAX_MAP_GEN_HEIGHT + TOP_BORDER_PADDING))
#define MAX_MAP_GEN_EVENTS 4
#define MAX_MAP_GEN_WARPS 4
#define MAX_MAP_GEN_COORD_EVENTS 4
#define MAX_MAP_GEN_BG_EVENTS 4
#define MAZE_DIR_NONE  0
#define MAZE_DIR_EAST  1 << 0
#define MAZE_DIR_SOUTH 1 << 1
#define MAZE_DIR_WEST  1 << 2
#define MAZE_DIR_NORTH 1 << 3

struct MazeNode {
    u8 x;
    u8 y;
    u8 dirs;
    u8 type;
    struct MazeNode *parent;
};

#define MAZE_NODE_WALL     (1 << 0)
#define MAZE_NODE_RESERVED (1 << 1)

EWRAM_DATA u16 gMapGenBorder[4] = {};
EWRAM_DATA u16 gMapGenMetatiles[MAX_MAP_GEN_WIDTH * MAX_MAP_GEN_HEIGHT] = {};
EWRAM_DATA struct EventObjectTemplate gMapGenEventObjects[MAX_MAP_GEN_EVENTS] = {};
EWRAM_DATA struct WarpEvent gMapGenWarps[4] = {};
EWRAM_DATA struct CoordEvent gMapGenCoordEvents[4] = {};
EWRAM_DATA struct BgEvent gMapGenBgEvents[4] = {};
EWRAM_DATA struct MapEvents gMapGenEvents = {0};
EWRAM_DATA struct MapLayout gMapGenLayout = {0};
EWRAM_DATA u16 gMapGenSeed = 0;
EWRAM_DATA struct MazeNode gMazeNodes[MAX_MAP_TILES] = {0};

const u8 gGeneratedMaps[][2] = {
    { MAP_NUM(HACK_GYM), MAP_GROUP(HACK_GYM) },
};

bool8 IsGeneratedMap(u8 mapNum, u8 mapGroup)
{
    int i;

    for (i = 0; i < ARRAY_COUNT(gGeneratedMaps); i++)
    {
        if (gGeneratedMaps[i][0] == mapNum && gGeneratedMaps[i][1] == mapGroup)
            return TRUE;
    }

    return FALSE;
}

static void InitMapEvents(void)
{
    DmaClear16(3, gMapGenEventObjects, MAX_MAP_GEN_EVENTS * sizeof(struct EventObjectTemplate))
    DmaClear16(3, gMapGenWarps, MAX_MAP_GEN_WARPS * sizeof(struct WarpEvent))
    DmaClear16(3, gMapGenCoordEvents, MAX_MAP_GEN_COORD_EVENTS * sizeof(struct CoordEvent))
    DmaClear16(3, gMapGenBgEvents, MAX_MAP_GEN_BG_EVENTS * sizeof(struct BgEvent))
    gMapGenEvents.eventObjectCount = 0;
    gMapGenEvents.warpCount = 0;
    gMapGenEvents.coordEventCount = 0;
    gMapGenEvents.bgEventCount = 0;
    gMapGenEvents.eventObjects = gMapGenEventObjects;
    gMapGenEvents.warps = gMapGenWarps;
    gMapGenEvents.coordEvents = gMapGenCoordEvents;
    gMapGenEvents.bgEvents = gMapGenBgEvents;
}

static struct MazeNode *Next(struct MazeNode *node, struct MazeNode *nodes, int mapWidth, int mapHeight)
{
    int dir;
    u8 x, y;
    struct MazeNode *dest;

    if (!node)
        return NULL;

    while (node->dirs)
    {
        do {
            dir = 1 << (Random() % 4);
        } while (!(node->dirs & dir));

        node->dirs &= ~dir;
        switch (dir)
        {
        case MAZE_DIR_EAST:
            if (node->x + 2 < mapWidth)
            {
                x = node->x + 2;
                y = node->y;
            }
            else
            {
                continue;
            }
            break;
        case MAZE_DIR_SOUTH:
            if (node->y + 2 < mapHeight)
            {
                x = node->x;
                y = node->y + 2;
            }
            else
            {
                continue;
            }
            break;
        case MAZE_DIR_WEST:
            if (node->x - 2 >= 0)
            {
                x = node->x - 2;
                y = node->y;
            }
            else
            {
                continue;
            }
            break;
        case MAZE_DIR_NORTH:
            if (node->y - 2 >= 0)
            {
                x = node->x;
                y = node->y - 2;
            }
            else
            {
                continue;
            }
            break;
        }

        dest = &nodes[x + y * mapWidth];
        if (!(dest->type & MAZE_NODE_WALL))
        {
            if (dest->parent)
            {
                continue;
            }
            dest->parent = node;
            nodes[node->x + (x - node->x) / 2 + (node->y + (y - node->y) / 2) * mapWidth].type &= ~MAZE_NODE_WALL;
            return dest;
        }
    }

    return node->parent;
}

// Heavily borrowed from https://en.wikipedia.org/wiki/Maze_generation_algorithm#C_code_example
static void GenMaze_RecursiveBacktracking(int mapWidth, int mapHeight)
{
    int i, j;
    struct MazeNode *start;
    struct MazeNode *cur;

    memset(gMazeNodes, 0, ARRAY_COUNT(gMazeNodes) * sizeof(struct MazeNode));

    // Init the grid of maze cells.
    for (i = 0; i < mapWidth; i++)
    {
        for (j = 0; j < mapHeight; j++)
        {
            struct MazeNode *node = &gMazeNodes[i + j * mapWidth];
            node->x = i;
            node->y = j;
            if ((i * j) % 2)
            {
                node->dirs = MAZE_DIR_NORTH | MAZE_DIR_EAST | MAZE_DIR_SOUTH | MAZE_DIR_WEST;
                node->type &= ~MAZE_NODE_WALL;
            }
            else
            {
                node->dirs = MAZE_DIR_NONE;
                node->type |= MAZE_NODE_WALL;
            }
        }
    }

    start = &gMazeNodes[1 + mapWidth];
    start->parent = start;
    cur = start;
    do
    {
        cur = Next(cur, gMazeNodes, mapWidth, mapHeight);
    } while (cur != start);
}

static int GetMetatile(int x, int y, int mapWidth, int mapHeight)
{
    if (!(gMazeNodes[x + y * mapWidth].type & MAZE_NODE_WALL))
    {
        return 0x210;
    }
    else
    {
        int metatiles[] = {
            0x217, // 0
            0x21A, // 1
            0x218, // 2
            0x219, // 3
            0x230, // 4
            0x224, // 5
            0x223, // 6
            0x21D, // 7
            0x220, // 8
            0x21C, // 9
            0x21B, // 10
            0x21E, // 11
            0x228, // 12
            0x21F, // 13
            0x241, // 14
            0x242, // 15
        };
        int score = 0;
        if (x > 0)
            score += (gMazeNodes[x - 1 + y * mapWidth].type & MAZE_NODE_WALL) ? 1 : 0;
        if (x < mapWidth - 1)
            score += (gMazeNodes[x + 1 + y * mapWidth].type & MAZE_NODE_WALL) ? 2 : 0;
        if (y > 0)
            score += (gMazeNodes[x + (y - 1) * mapWidth].type & MAZE_NODE_WALL) ? 4 : 0;
        if (y < mapHeight - 1)
            score += (gMazeNodes[x + (y + 1) * mapWidth].type & MAZE_NODE_WALL) ? 8 : 0;

        return metatiles[score];
    }
}

struct MazeTrainer
{
    u8 gfxId;
    u8 *script;
};

static void PlaceMazeEventObjects(struct MazeNode *nodes, int mapWidth, int mapHeight, int *eventObjectCount)
{
    int i = 0;
    int numTrainers = 2;
    const struct MazeTrainer trainers[] =
    {
        {
            .gfxId = EVENT_OBJ_GFX_CAMPER,
            .script = S_HackGym_Trainer1,
        },
        {
            .gfxId = EVENT_OBJ_GFX_MAN_6,
            .script = S_HackGym_Trainer2,
        },
    };

    while (i < numTrainers)
    {
        int x = Random() % mapWidth;
        int y = Random() % mapHeight;
        int index = x + y * mapWidth;
        if (nodes[index].type & MAZE_NODE_RESERVED || nodes[index].type & MAZE_NODE_WALL)
            continue;

        nodes[index].type |= MAZE_NODE_RESERVED;
        // Free up the immediate surrounding tiles to avoid trapping the player.
        nodes[index - mapWidth - 1].type &= ~MAZE_NODE_WALL;
        nodes[index - mapWidth + 0].type &= ~MAZE_NODE_WALL;
        nodes[index - mapWidth + 1].type &= ~MAZE_NODE_WALL;
        nodes[index - 1].type &= ~MAZE_NODE_WALL;
        nodes[index + 1].type &= ~MAZE_NODE_WALL;
        nodes[index + mapWidth - 1].type &= ~MAZE_NODE_WALL;
        nodes[index + mapWidth + 0].type &= ~MAZE_NODE_WALL;
        nodes[index + mapWidth + 1].type &= ~MAZE_NODE_WALL;

        gMapGenEventObjects[*eventObjectCount].localId = i + 1;
        gMapGenEventObjects[*eventObjectCount].graphicsId = trainers[i].gfxId;
        gMapGenEventObjects[*eventObjectCount].x = x;
        gMapGenEventObjects[*eventObjectCount].y = y + TOP_BORDER_PADDING;
        gMapGenEventObjects[*eventObjectCount].elevation = 3;
        gMapGenEventObjects[*eventObjectCount].movementType = MOVEMENT_TYPE_FACE_DOWN;
        gMapGenEventObjects[*eventObjectCount].movementRangeX = 0;
        gMapGenEventObjects[*eventObjectCount].movementRangeY = 0;
        gMapGenEventObjects[*eventObjectCount].trainerType = 1;
        gMapGenEventObjects[*eventObjectCount].trainerRange_berryTreeId = 1;
        gMapGenEventObjects[*eventObjectCount].script = trainers[i].script;
        gMapGenEventObjects[*eventObjectCount].flagId = 0;
        (*eventObjectCount)++;

        i++;
    }
}

static void PlaceMazeCoordEvents(struct MazeNode *nodes, int mapWidth, int mapHeight, int *coordEventCount)
{
    int i = 0;
    int numRegenTiles = MAX_MAP_GEN_COORD_EVENTS;

    while (i < numRegenTiles)
    {
        int x = Random() % mapWidth;
        int y = Random() % mapHeight;
        int index = x + y * mapWidth;
        if (nodes[index].type & MAZE_NODE_RESERVED || nodes[index].type & MAZE_NODE_WALL)
            continue;

        nodes[index].type |= MAZE_NODE_RESERVED;
        gMapGenCoordEvents[*coordEventCount].x = x;
        gMapGenCoordEvents[*coordEventCount].y = y + TOP_BORDER_PADDING;
        gMapGenCoordEvents[*coordEventCount].elevation = 3;
        gMapGenCoordEvents[*coordEventCount].trigger = VAR_TEMP_0;
        gMapGenCoordEvents[*coordEventCount].index = 0;
        gMapGenCoordEvents[*coordEventCount].script = S_HackGym_Regenerate;
        (*coordEventCount)++;

        gMapGenMetatiles[x + (y + TOP_BORDER_PADDING) * mapWidth] = (3 << 12) | (0 << 10) | 0x20F;

        i++;
    }
}

void Task_GenerateMap(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 0:
        data[1] += 1;
        if (data[1] > 14)
        {
            int i, j;
            data[0]++;
            PlaySE(SE_FU_ZUZUZU);
            GenerateMapFromSeed(Random());
            for (i = 0; i < gMapGenLayout.width; i++)
            {
                for (j = 0; j < gMapGenLayout.height; j++)
                {
                    int x = i + 7;
                    int y = j + 7;
                    MapGridSetMetatileIdAt(x, y, gMapGenMetatiles[i + j * gMapGenLayout.width]);
                }
            }
            DrawWholeMapView();
            RemoveNonPlayerEventObjects();
            LoadEventObjTemplatesFromHeader();
            TrySpawnEventObjects(0, 0);
        }
        break;
    case 1:
        data[1] -= 1;
        if (data[1] == 0)
        {
            data[0]++;
        }
        break;
    case 2:
        DestroyTask(taskId);
        return;
    }

    BlendPalette(0, 0x100, data[1] / 2, RGB(14, 0, 14));
    REG_MOSAIC = ((data[1] / 2) << 4) | data[1];
}

bool8 WaitGenerateMap(void)
{
    return !FuncIsActiveTask(Task_GenerateMap);
}

void GenerateMapFromSeed(int seed)
{
    gSaveBlock1.mapGenSeed = seed;
    GenerateMap();
}

void GenerateMap(void)
{
    int i, j;
    u16 playerX;
    u16 playerY;
    int mapWidth, mapHeight;
    int topBorderOffset;
    int eventObjectCount, warpCount, coordEventCount, bgEventCount;
    int exitWarpX, exitWarpY, exitWarpIndex;
    int gymLeaderX, gymLeaderY;

    gDisableRNGAdvance = TRUE;
    PlayerGetDestCoords(&playerX, &playerY);
    playerX -= 7;
    playerY -= 7;
    SeedRng(gSaveBlock1.mapGenSeed);
    mapWidth = 19;
    mapHeight = 19;
    topBorderOffset = TOP_BORDER_PADDING * mapWidth;
    eventObjectCount = 0;
    warpCount = 0;
    coordEventCount = 0;
    bgEventCount = 0;

    InitMapEvents();

    gMapGenLayout.width = mapWidth ;
    gMapGenLayout.height = mapHeight + TOP_BORDER_PADDING;
    gMapGenLayout.border = gMapGenBorder;
    gMapGenLayout.map = gMapGenMetatiles;
    gMapGenBorder[0] = 0x0208;
    gMapGenBorder[1] = 0x0208;
    gMapGenBorder[2] = 0x0208;
    gMapGenBorder[3] = 0x0208;
    gMapGenLayout.primaryTileset = &gTileset_Building;
    gMapGenLayout.secondaryTileset = &gTileset_MossdeepGym;

    GenMaze_RecursiveBacktracking(mapWidth, mapHeight);

    // Make sure entrance has openings around it.
    exitWarpX = mapWidth / 2 - 1;
    exitWarpY = mapHeight - 1;
    for (i = -2; i < 4; i++)
    {
        for (j = -3; j < 1; j++)
        {
            int index = (exitWarpX + i) + (exitWarpY + j) * mapWidth;
            gMazeNodes[index].type &= ~MAZE_NODE_WALL;
            gMazeNodes[index].type |= MAZE_NODE_RESERVED;
        }
    }

    // Carve out open area for Gym Leader
    gymLeaderX = mapWidth / 2;
    gymLeaderY = 2 + TOP_BORDER_PADDING;
    for (i = gymLeaderX - 3; i < gymLeaderX + 3; i++)
    {
        for (j = 1; j < 6; j++)
        {
            int index = i + j * mapWidth;
            gMazeNodes[index].type &= ~MAZE_NODE_WALL;
            gMazeNodes[index].type |= MAZE_NODE_RESERVED;
        }
    }

    // Make top row "open" so marching squares tiles look correct.
    for (i = 0; i < mapWidth; i++)
    {
        gMazeNodes[i].type &= ~MAZE_NODE_WALL;
        gMazeNodes[i].type |= MAZE_NODE_RESERVED;
    }

    // Mark the player's current position as reserved and open
    gMazeNodes[playerX + (playerY - TOP_BORDER_PADDING) * mapWidth].type &= ~MAZE_NODE_WALL;
    gMazeNodes[playerX + (playerY - TOP_BORDER_PADDING) * mapWidth].type |= MAZE_NODE_RESERVED;

    PlaceMazeEventObjects(gMazeNodes, mapWidth, mapHeight, &eventObjectCount);

    for (i = 0; i < mapWidth * mapHeight; i++)
    {
        int elevation = 3;
        int collision = (gMazeNodes[i].type & MAZE_NODE_WALL) ? 1 : 0;
        int metatile = GetMetatile(i % mapWidth, i / mapHeight, mapWidth, mapHeight);
        gMapGenMetatiles[i + topBorderOffset] = (elevation << 12) | (collision << 10) | metatile;
    }

    // Exit pads
    exitWarpIndex = exitWarpY * mapWidth + exitWarpX + topBorderOffset;
    gMapGenMetatiles[exitWarpIndex] = (3 << 12) | (0 << 10) | 0x221;
    gMapGenMetatiles[exitWarpIndex + 1] = (3 << 12) | (0 << 10) | 0x222;

    // Gym Statues
    gMapGenMetatiles[exitWarpIndex - 1 * mapWidth - 1] = (3 << 12) | (0 << 10) | 0x23B;
    gMapGenMetatiles[exitWarpIndex - 2 * mapWidth - 1] = (0 << 12) | (1 << 10) | 0x244;
    gMapGenMetatiles[exitWarpIndex - 3 * mapWidth - 1] = (3 << 12) | (0 << 10) | 0x23C;
    gMapGenMetatiles[exitWarpIndex - 1 * mapWidth + 2] = (3 << 12) | (0 << 10) | 0x23B;
    gMapGenMetatiles[exitWarpIndex - 2 * mapWidth + 2] = (0 << 12) | (1 << 10) | 0x244;
    gMapGenMetatiles[exitWarpIndex - 3 * mapWidth + 2] = (3 << 12) | (0 << 10) | 0x23C;

    // North Gym wall
    for (i = 0; i < mapWidth; i++)
        gMapGenMetatiles[i] = (0 << 12) | (1 << 10) | 0x240;
    for (i = mapWidth; i < 2 * mapWidth; i++)
        gMapGenMetatiles[i] = (0 << 12) | (1 << 10) | 0x248;
    for (i = mapWidth; i < 2 * mapWidth; i++)
    {
        if (!(gMazeNodes[i].type & MAZE_NODE_WALL))
            gMapGenMetatiles[i + mapWidth * TOP_BORDER_PADDING] = (3 << 12) | (0 << 10) | 0x211;
    }

    gMapGenWarps[warpCount].x = exitWarpX;
    gMapGenWarps[warpCount].y = exitWarpY + TOP_BORDER_PADDING;
    gMapGenWarps[warpCount].elevation = gMapGenMetatiles[exitWarpIndex] >> 12;
    gMapGenWarps[warpCount].warpId = 0;
    gMapGenWarps[warpCount].mapNum = MAP_NUM(LITTLEROOT_TOWN);
    gMapGenWarps[warpCount].mapGroup = MAP_GROUP(LITTLEROOT_TOWN);
    warpCount++;

    gMapGenWarps[warpCount].x = exitWarpX + 1;
    gMapGenWarps[warpCount].y = exitWarpY + TOP_BORDER_PADDING;
    gMapGenWarps[warpCount].elevation = gMapGenMetatiles[exitWarpIndex + 1] >> 12;
    gMapGenWarps[warpCount].warpId = 0;
    gMapGenWarps[warpCount].mapNum = MAP_NUM(LITTLEROOT_TOWN);
    gMapGenWarps[warpCount].mapGroup = MAP_GROUP(LITTLEROOT_TOWN);
    warpCount++;

    gMapGenBgEvents[bgEventCount].x = exitWarpX - 1;
    gMapGenBgEvents[bgEventCount].y = exitWarpY - 2 + TOP_BORDER_PADDING;
    gMapGenBgEvents[bgEventCount].elevation = 3;
    gMapGenBgEvents[bgEventCount].kind = BG_EVENT_PLAYER_FACING_NORTH;
    gMapGenBgEvents[bgEventCount].bgUnion.script = S_HackGym_Statue;
    bgEventCount++;

    gMapGenBgEvents[bgEventCount].x = exitWarpX + 2;
    gMapGenBgEvents[bgEventCount].y = exitWarpY - 2 + TOP_BORDER_PADDING;
    gMapGenBgEvents[bgEventCount].elevation = 3;
    gMapGenBgEvents[bgEventCount].kind = BG_EVENT_PLAYER_FACING_NORTH;
    gMapGenBgEvents[bgEventCount].bgUnion.script = S_HackGym_Statue;
    bgEventCount++;

    gMapGenEventObjects[eventObjectCount].localId = 0;
    gMapGenEventObjects[eventObjectCount].graphicsId = EVENT_OBJ_GFX_WATTSON;
    gMapGenEventObjects[eventObjectCount].x = gymLeaderX;
    gMapGenEventObjects[eventObjectCount].y = gymLeaderY;
    gMapGenEventObjects[eventObjectCount].elevation = 3;
    gMapGenEventObjects[eventObjectCount].movementType = MOVEMENT_TYPE_FACE_DOWN;
    gMapGenEventObjects[eventObjectCount].movementRangeX = 0;
    gMapGenEventObjects[eventObjectCount].movementRangeY = 0;
    gMapGenEventObjects[eventObjectCount].trainerType = 0;
    gMapGenEventObjects[eventObjectCount].trainerRange_berryTreeId = 0;
    gMapGenEventObjects[eventObjectCount].script = S_HackGym_Leader;
    gMapGenEventObjects[eventObjectCount].flagId = 0;
    eventObjectCount++;

    PlaceMazeCoordEvents(gMazeNodes, mapWidth, mapHeight, &coordEventCount);

    gMapGenEvents.eventObjectCount = eventObjectCount;
    gMapGenEvents.warpCount = warpCount;
    gMapGenEvents.coordEventCount = coordEventCount;
    gMapGenEvents.bgEventCount = bgEventCount;

    gDisableRNGAdvance = FALSE;
}
