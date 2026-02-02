//
// Created by os on 12/22/24.
//

#ifndef RAID_H
#define RAID_H
#include "types.h"


typedef struct{
  uchar raidType;
  uchar numOfDisks;//num of disks which implement raid structure
  uchar numOfBlocks; //num of blocks on one disk
  uchar isInitialized; //is raid already initialiazed
}Raid;

typedef struct{
  uint diskID;
  uint valid; //is disk valid (not failed)
  uint pair; //number (diskID) of disk copy
}Disk;


extern Raid* raidStructure;
extern struct sleeplock* lock;
extern uint BLOCK_SIZE;
extern uint DISKS_NUM;
extern Disk disks[];
extern uint DISK_SIZE; //velicina diska u bajtovima (128M)

#endif //RAID_H
