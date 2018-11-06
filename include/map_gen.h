#ifndef GUARD_MAP_GEN_H
#define GUARD_MAP_GEN_H

bool8 IsGeneratedMap(u8 mapNum, u8 mapGroup);
void GenerateMap(void);
void GenerateMapFromSeed(int seed);
void Task_GenerateMap(u8 taskId);
bool8 WaitGenerateMap(void);

#endif //GUARD_MAP_GEN_H
