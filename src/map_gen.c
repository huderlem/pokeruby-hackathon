#include "global.h"
#include "map_gen.h"
#include "random.h"
#include "constants/map_types.h"
#include "constants/maps.h"
#include "constants/songs.h"
#include "constants/weather.h"
#include "constants/region_map_sections.h"

extern struct Tileset gTileset_General;
extern struct Tileset gTileset_Building;
extern struct Tileset gTileset_PetalburgGym;

#define MIN_MAP_GEN_WIDTH 8
#define MIN_MAP_GEN_HEIGHT 8
#define MAX_MAP_GEN_WIDTH 20
#define MAX_MAP_GEN_HEIGHT 20
#define MAX_MAP_GEN_EVENTS 4
#define MAX_MAP_GEN_WARPS 4
#define MAX_MAP_GEN_COORD_EVENTS 4
#define MAX_MAP_GEN_BG_EVENTS 4

EWRAM_DATA u16 gMapGenBorder[4] = {};
EWRAM_DATA u16 gMapGenMetatiles[MAX_MAP_GEN_WIDTH * MAX_MAP_GEN_HEIGHT] = {};
EWRAM_DATA struct EventObjectTemplate gMapGenEventObjects[MAX_MAP_GEN_EVENTS] = {};
EWRAM_DATA struct WarpEvent gMapGenWarps[4] = {};
EWRAM_DATA struct CoordEvent gMapGenCoordEvents[4] = {};
EWRAM_DATA struct BgEvent gMapGenBgEvents[4] = {};
EWRAM_DATA struct MapEvents gMapGenEvents = {0};
EWRAM_DATA struct MapLayout gMapGenLayout = {0};
EWRAM_DATA u16 gMapGenSeed = 0;

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

static void InitMapHeader(void)
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

void GenerateMap(void)
{
    int i;
    int mapWidth, mapHeight;
    int eventObjectCount, warpCount, coordEventCount, bgEventCount;
    int exitWarpX, exitWarpY, exitWarpIndex;

    const u16 openFloorTiles[] = { 0x201, 0x209, 0x209, 0x209, 0x209 };

    SeedRng(gMapGenSeed);
    mapWidth = 8;
    mapHeight = 8;
    eventObjectCount = 0;
    warpCount = 0;
    coordEventCount = 0;
    bgEventCount = 0;

    InitMapHeader();

    gMapGenLayout.width = mapWidth;
    gMapGenLayout.height = mapHeight;
    gMapGenLayout.border = gMapGenBorder;
    gMapGenLayout.map = gMapGenMetatiles;
    gMapGenBorder[0] = 0x0208;
    gMapGenBorder[1] = 0x0208;
    gMapGenBorder[2] = 0x0208;
    gMapGenBorder[3] = 0x0208;
    gMapGenLayout.primaryTileset = &gTileset_Building;
    gMapGenLayout.secondaryTileset = &gTileset_PetalburgGym;

    for (i = 0; i < mapWidth * mapHeight; i++)
    {
        gMapGenMetatiles[i] = (3 << 12) | (0 << 10) | openFloorTiles[Random() % ARRAY_COUNT(openFloorTiles)];
    }

    exitWarpX = mapWidth / 2 - 1;
    exitWarpY = mapHeight - 1;
    exitWarpIndex = exitWarpY * mapWidth + exitWarpX;
    gMapGenMetatiles[exitWarpIndex] = (3 << 12) | (0 << 10) | 0x006;
    gMapGenMetatiles[exitWarpIndex + 1] = (3 << 12) | (0 << 10) | 0x007;

    gMapGenWarps[warpCount].x = exitWarpX;
    gMapGenWarps[warpCount].y = exitWarpY;
    gMapGenWarps[warpCount].elevation = gMapGenMetatiles[exitWarpIndex] >> 12;
    gMapGenWarps[warpCount].warpId = 0;
    gMapGenWarps[warpCount].mapNum = MAP_NUM(LITTLEROOT_TOWN);
    gMapGenWarps[warpCount].mapGroup = MAP_GROUP(LITTLEROOT_TOWN);
    warpCount++;

    gMapGenWarps[warpCount].x = exitWarpX + 1;
    gMapGenWarps[warpCount].y = exitWarpY;
    gMapGenWarps[warpCount].elevation = gMapGenMetatiles[exitWarpIndex + 1] >> 12;
    gMapGenWarps[warpCount].warpId = 0;
    gMapGenWarps[warpCount].mapNum = MAP_NUM(LITTLEROOT_TOWN);
    gMapGenWarps[warpCount].mapGroup = MAP_GROUP(LITTLEROOT_TOWN);
    warpCount++;

    gMapGenEvents.eventObjectCount = eventObjectCount;
    gMapGenEvents.warpCount = warpCount;
    gMapGenEvents.coordEventCount = coordEventCount;
    gMapGenEvents.bgEventCount = bgEventCount;
    gMapGenSeed = Random();
}
