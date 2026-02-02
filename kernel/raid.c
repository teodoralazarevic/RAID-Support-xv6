//
// Created by os on 12/22/24.
//
#include "raid.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"

uint BLOCK_SIZE = BSIZE;
uint DISKS_NUM = DISKS; //UKUPNO, SA KOPIJAMA
Disk disks[DISKS];
uint DISK_SIZE = 1 << 18;
Raid* raidStructure=0;
struct sleeplock* lock=0;


//writing in 0. block types of uchar (info about raid structure)
void write_raid_info(int disk){
  //raid info is written in first(0) block on every disk in system
//kalloc allocates one page of memory
	//printf("Write raid info started\n");
	/*printf("%d ", raidStructure->raidType);
    printf("%d ",raidStructure->numOfDisks);
    printf("%d ",raidStructure->numOfBlocks);
    printf("%d ",raidStructure->isInitialized);
	printf("%d ", disk);*/
	write_block(disk, 0, (uchar*)raidStructure);
}


//check raid info from disk to see if raid is initializied
int check_raid_init(){
	if(raidStructure==0){
		return 0;
	}
	//printf("Check raid init\n");
    return raidStructure->isInitialized==1;
}


int init_raid(enum RAID_TYPE type) {
	//printf("Init raid started\n");
  if(check_raid_init())
	return -1; //raid already initalizied
  if(lock==0){
      lock=(struct sleeplock*)kalloc();
      initsleeplock(lock, "myLock");
  }
  //global structure
  raidStructure=kalloc();
  raidStructure->raidType = type;
  raidStructure->isInitialized = 1;
  raidStructure->numOfBlocks = DISK_SIZE/BLOCK_SIZE-1;   // broj blokova po disku
  raidStructure->numOfDisks  = DISKS_NUM;
  //if num of disks is odd and raid1 or raid0+1 is used
  if((type==RAID1 || type==RAID0_1) && DISKS_NUM%2)
    	DISKS_NUM--;
  //on every disk raid structure is written in 0. block
  for(int i=0;i<DISKS_NUM;i++){
    write_raid_info(i+1); //disk numbers starts from 1
    disks[i].diskID=i+1;
    disks[i].valid=1;
    if(type==RAID1 || type==RAID0_1){
		if(disks[i].diskID<=DISKS_NUM/2)
            disks[i].pair=disks[i].diskID+DISKS_NUM/2;
        else
  	     	disks[i].pair=disks[i].diskID-DISKS_NUM/2;
    }
    else{
      disks[i].pair=0; //disk doesnt have pair
    }
  }
    //printf("Racunam broj blokova");
    uint numOfBlocks=DISK_SIZE/BLOCK_SIZE-1;
    uchar *buf = (uchar*)kalloc();
    if (!buf)
        return -1;
    memset(buf, 0, BLOCK_SIZE); //fill block with zeros
    //printf("Blok setovan na 0");
    for(uint diskn=1;diskn<=DISKS_NUM;diskn++){ //every block on every disc fill with zeros (not first - 0 block)
        for(uint i=1;i<numOfBlocks;i++) {
      	    write_block(diskn, i, buf);
        }
    }
    kfree(buf);
    //printf("Init raid finished\n");
  return 0;
}

int disk_fail_raid(int diskn){
    acquiresleep(lock);
	//printf("POKVARIO SE DISK %d\n", diskn);
  	if(diskn<1 || diskn>DISKS_NUM){
          releasesleep(lock);
          return -1;
    }
   	if(raidStructure->isInitialized==0 && check_raid_init()==0){
          releasesleep(lock);
          return -2;
    }
	disks[diskn-1].valid=0;
    //printf("Disk fail raid finished\n");
    releasesleep(lock);
  	return 0;
}

int myMemCopy(uint diskDestination, uint diskSource, uint numOfBlocks){
  	for(uint i=0;i<numOfBlocks;i++){
          uchar* mem=(uchar*)kalloc();
          if(!mem)
            return -1;
          read_block(diskSource, i, mem);
          write_block(diskDestination, i, mem);
          kfree(mem);
  	}
    return 0;
}

int reconstruct_block(int disk, int block, uchar* data){
    /*uchar* parityBlock = (uchar*)kalloc();
    if (!parityBlock) return -1;
    read_block(DISKS_NUM, block, parityBlock);*/
    if(!disks[DISKS_NUM-1].valid)
        return -1;
    uchar* tmp=(uchar*)kalloc();
    if(!tmp)
        return -1;
    memset(data, 0, BLOCK_SIZE);
    int multipleDisksFailed=0;
    for (int d = 1; d <= DISKS_NUM; d++) {
        if (d == disk) continue;
        if(disks[d-1].valid==0) {
            multipleDisksFailed=-1; //ne moze rekonstrukcija ako je jos neki pokvaren
            break;
        }
        read_block(d, block, tmp);
        for (int i = 0; i < BLOCK_SIZE; i++) {
            data[i] ^= tmp[i];
        }
    }
    kfree(tmp);
    //printf("\tREKONSTRUISAN je podatak %d disk %d, blok %d\n", data[0], disk, block);
    return multipleDisksFailed;
}

int disk_repaired_raid(int diskn){
    acquiresleep(lock);
	//printf("Disk repaired raid started\n");
	if(diskn<1 || diskn>DISKS_NUM){
        releasesleep(lock);
        return -1;
    }
    if(raidStructure->isInitialized==0 && check_raid_init()==0){
        releasesleep(lock);
        return -2;
    }
    Disk disk=disks[diskn-1];
    if(disk.valid==1){ //disk is already valid
      	releasesleep(lock);
        return 0;
    }

    uint numOfBlocks=DISK_SIZE/BLOCK_SIZE-1;
    if(raidStructure->raidType==RAID4 || raidStructure->raidType==RAID5){
        for(int i=0;i<numOfBlocks;i++){
            uchar* buffer=(uchar*)kalloc();
            reconstruct_block(diskn, i, buffer);
            write_block(diskn, i, buffer);
        }
        releasesleep(lock);
        return 0;
    }
    //0 is written in every disk block

    uchar *buf = (uchar*)kalloc();
    if (!buf){
        releasesleep(lock);
        return -1;
    }
    memset(buf, 0, BLOCK_SIZE);
    //printf("Setovan je blok na sve 0");
    write_raid_info(diskn);
    //printf("Upisan je nulti blok na disk");
    for(uint i=1;i<numOfBlocks;i++) {
      	write_block(diskn, i, buf);
    }
    //printf("Upisane su nule svuda");
    kfree(buf);
    disks[diskn-1].valid=1;
    //if disk has a valid copy
   	if(disk.pair!=0 && disks[disk.pair-1].valid==1){
          int ret = myMemCopy(diskn, disk.pair, numOfBlocks);
          releasesleep(lock);
          return ret;
   	}
    //printf("Disk repaired raid finished\n");
    releasesleep(lock);
    return 0;
}

int info_raid(uint *blkn, uint *blks, uint *diskn){
	//printf("Info raid started\n");
    acquiresleep(lock);
  	if(raidStructure->isInitialized==0 && check_raid_init()==0){
        releasesleep(lock);
        return -2;
    }
	uint numOfBlocks=raidStructure->numOfDisks * raidStructure->numOfBlocks;
    if(raidStructure->raidType==RAID1 || raidStructure->raidType==RAID0_1)
      	numOfBlocks/=2;
    *blkn=numOfBlocks;
    *blks=BLOCK_SIZE;
    *diskn=raidStructure->numOfDisks;
    //printf("Info raid finished: %d %d %d\n", *blkn, *blks, *diskn);
    releasesleep(lock);
  	return 0;
}

int write_raid(int blkn, uchar* data){
    //printf("write raid started %d\n", blkn);
    acquiresleep(lock);
    if(raidStructure->raidType==RAID0){
        int disk=blkn%DISKS_NUM+1;
        int block=blkn/DISKS_NUM+1;
        if(disk<0 || disk>DISKS_NUM || disks[disk-1].valid==0){
            releasesleep(lock);
            return -1;
        }
        write_block(disk, block, data);
        //printf("\tUpisan je podatak %d u disk %d, blok %d\n", data[0], disk, block);
        releasesleep(lock);
        return 0;
    }
    else if(raidStructure->raidType==RAID1){
        int numOfDisks=raidStructure->numOfDisks/2; //number of original disks
        int numOfBlocks=DISK_SIZE/BLOCK_SIZE-1; //number OD blocks on disk
        int diskOriginal=blkn/numOfBlocks+1;
        int block=blkn%numOfBlocks+1;
        int diskCopy=disks[diskOriginal-1].pair;
        //printf("Original %d\n", diskOriginal);
        //printf("Kopija %d\n", diskCopy);
        if(diskOriginal<0 || diskOriginal>numOfDisks){
            releasesleep(lock);
            return -1;
        }
        int written=0; //is written in any disk
        if(disks[diskOriginal-1].valid==1) {
            write_block(diskOriginal, block, data);
            written=1;
            //printf("Upisan je podatak %d u disk %d, blok %d\n\n", data[0], diskOriginal, block);
        }
        if(disks[diskCopy-1].valid==1){
            write_block(diskCopy, block, data);
            written=1;
            //printf("Upisan je podatak %d u disk %d, blok %d\n", data[0], diskCopy, block);
        }
        if(written==0){
            releasesleep(lock);
            return -1;
        }
        releasesleep(lock);
        return 0;
    }
    else if(raidStructure->raidType==RAID0_1){
        int numOfDisks=raidStructure->numOfDisks/2; //number of original disks
        //int numOfBlocks=DISK_SIZE/BLOCK_SIZE-1; //number od blocks on disk
        int diskOriginal=blkn%(numOfDisks)+1;
        int block=blkn/numOfDisks+1;
        int diskCopy=disks[diskOriginal-1].pair;
        //printf("Original %d\n", diskOriginal);
        //printf("Kopija %d\n", diskCopy);
        if(diskOriginal<0 || diskOriginal>numOfDisks){
            releasesleep(lock);
            return -1;
        }
        int written=0; //is written in any disk
        if(disks[diskOriginal-1].valid==1) {
            write_block(diskOriginal, block, data);
            written=1;
            //printf("Upisan je podatak %d u disk %d, blok %d\n\n", data[0], diskOriginal, block);
        }
        if(disks[diskCopy-1].valid==1){
            write_block(diskCopy, block, data);
            written=1;
            //printf("Upisan je podatak %d u disk %d, blok %d\n", data[0], diskCopy, block);
        }
        if(written==0){
            releasesleep(lock);
            return -1;
        }
        releasesleep(lock);
        return 0;
    }
    else if(raidStructure->raidType==RAID4){
        int numOfDisksRaid4=DISKS_NUM-1;
        int disk=blkn%numOfDisksRaid4+1;
        int block=blkn/numOfDisksRaid4+1;

        if(disk<0 || disk>=DISKS_NUM){
            releasesleep(lock);
            return -1;
        }

        int parityDisk=DISKS_NUM;

        uchar* oldParity=(uchar*)kalloc();
        uchar* oldBlock=(uchar*)kalloc();
        uchar* newParity=(uchar*)kalloc();

        if(disks[disk-1].valid==1)
            read_block(disk, block, oldBlock); //old block content
        else
            reconstruct_block(disk, block, oldBlock);
        read_block(parityDisk, block, oldParity); //old parity content

        for(int i=0;i<BLOCK_SIZE;i++) {
            newParity[i]=oldParity[i]^oldBlock[i]^data[i];
        }
        if(disks[disk-1].valid==1){
            write_block(disk, block, data);
            //printf("\tUpisan je podatak %d u disk %d, blok %d\n", data[0], disk, block);
        }
        write_block(DISKS_NUM, block, newParity);

        kfree(oldParity);
        kfree(oldBlock);
        kfree(newParity);
        releasesleep(lock);
        return 0;
    }
    else if(raidStructure->raidType==RAID5){
        int block, disk;
        block=blkn/(DISKS_NUM-1)+1;
        disk=blkn%(DISKS_NUM-1)+1;
        int parityDisk=(block-1)%DISKS_NUM+1;
        if(disk>=parityDisk)
            disk++;

        if(disk<0 || disk>DISKS_NUM){
            releasesleep(lock);
            return -1;
        }

        //int parityDisk=block; //na ovom disku je parity, u tom bloku
        int parityBlock=block;

        uchar* oldParity=(uchar*)kalloc();
        uchar* oldBlock=(uchar*)kalloc();
        uchar* newParity=(uchar*)kalloc();

        if(disks[disk-1].valid==1)
            read_block(disk, block, oldBlock); //old block content
        else
            reconstruct_block(disk, block, oldBlock);
        read_block(parityDisk, parityBlock, oldParity); //old parity content

        for(int i=0;i<BLOCK_SIZE;i++) {
            newParity[i]=oldParity[i]^oldBlock[i]^data[i];
        }
        if(disks[disk-1].valid==1){
            write_block(disk, block, data);
            //printf("\tUpisan je podatak %d u disk %d, blok %d\n", data[0], disk, block);
        }
        write_block(parityDisk, parityBlock, newParity);

        kfree(oldParity);
        kfree(oldBlock);
        kfree(newParity);
        releasesleep(lock);
        return 0;
    }

    //printf("write raid finished\n");
    releasesleep(lock);
    return -1;
}

int read_raid(int blkn, uchar* data){
    //printf("read raid started %d\n", blkn);
    acquiresleep(lock);
    if(raidStructure->raidType==RAID0){
        int disk=blkn%DISKS_NUM+1;
        int block=blkn/DISKS_NUM+1;
        if(disk<1 || disk>DISKS_NUM || disks[disk-1].valid==0){
            releasesleep(lock);
            return -1;
        }
        read_block(disk, block, data);
        //printf("\tProcitan je podatak %d iz diska %d, blok %d\n", data[0], disk, block);
        releasesleep(lock);
        return 0;
    }
    else if(raidStructure->raidType==RAID1){
        int numOfDisks=raidStructure->numOfDisks/2; //number of original disks
        int numOfBlocks=DISK_SIZE/BLOCK_SIZE-1; //number od blocks on disk
        int diskOriginal=blkn/numOfBlocks+1;
        int block=blkn%numOfBlocks+1;
        int diskCopy=disks[diskOriginal-1].pair;
        //printf("Original %d", diskOriginal);
        //printf("Kopija %d", diskCopy);
        if(diskOriginal<0 || diskOriginal>numOfDisks){
            releasesleep(lock);
            return -1;
        }
        if(disks[diskOriginal-1].valid==1) {
            read_block(diskOriginal, block, data);
            //printf("Procitan je podatak %d iz diska %d, blok %d\n", data[0], diskOriginal, block);
            releasesleep(lock);
            return 0;
        }
        if(disks[diskCopy-1].valid==1){
            read_block(diskCopy, block, data);
            //printf("Procitan je podatak %d iz diska %d MIRROSLAV, blok %d\n", data[0], diskCopy, block);
            releasesleep(lock);
            return 0;
        }
    }
    else if(raidStructure->raidType==RAID0_1){
        int numOfDisks=raidStructure->numOfDisks/2; //number of original disks
        //int numOfBlocks=DISK_SIZE/BLOCK_SIZE-1; //number od blocks on disk
        int diskOriginal=blkn%(numOfDisks)+1;
        int block=blkn/numOfDisks+1;
        int diskCopy=disks[diskOriginal-1].pair;
        //printf("Original %d", diskOriginal);
        //printf("Kopija %d", diskCopy);
        if(diskOriginal<0 || diskOriginal>numOfDisks){
            releasesleep(lock);
            return -1;
        }
        if(disks[diskOriginal-1].valid==1) {
            read_block(diskOriginal, block, data);
            //printf("Procitan je podatak %d iz diska %d, blok %d\n", data[0], diskOriginal, block);
            releasesleep(lock);
            return 0;
        }
        if(disks[diskCopy-1].valid==1){
            read_block(diskCopy, block, data);
            //printf("Procitan je podatak %d iz diska %d, blok %d\n", data[0], diskCopy, block);
            releasesleep(lock);
            return 0;
        }
    }
    else if(raidStructure->raidType==RAID4){
        int numOfDisksRaid4=DISKS_NUM-1;
        int disk=blkn%numOfDisksRaid4+1;
        int block=blkn/numOfDisksRaid4+1;
        if(disk<1 || disk>=DISKS_NUM){
            releasesleep(lock);
            return -1;
        }
        if(disks[disk-1].valid==1) {
            read_block(disk, block, data);
            //printf("\tProcitan je podatak %d disk %d, blok %d\n", data[0], disk, block);
            releasesleep(lock);
            return 0;
        }
        else{
            int ret=reconstruct_block(disk, block, data);
            releasesleep(lock);
            return ret;
        }

        //printf("\tProcitan je podatak %d iz diska %d, blok %d\n", data[0], disk, block);

    }
    else if(raidStructure->raidType==RAID5){
        int block, disk;
        block=blkn/(DISKS_NUM-1)+1;
        disk=blkn%(DISKS_NUM-1)+1;
        int parity=(block-1)%DISKS_NUM+1;
        if(disk>=parity)
            disk++;
        if(disk<1 || disk>DISKS_NUM){
            releasesleep(lock);
            return -1;
        }
        if(disks[disk-1].valid==1) {
            read_block(disk, block, data);
            releasesleep(lock);
            return 0;
        }
        else{
            int ret=reconstruct_block(disk, block, data);
            releasesleep(lock);
            return ret;
        }

        //printf("\tProcitan je podatak %d iz diska %d, blok %d\n", data[0], disk, block);

    }
    //printf("read raid started\n");
    releasesleep(lock);
    return -1;
}


int destroy_raid(){
	//printf("Destroy raid\n");
    acquiresleep(lock);
  	if(raidStructure->isInitialized==0 && check_raid_init()==0){
        releasesleep(lock);
        return -2;
    }
  	uint cnt=raidStructure->numOfDisks;
  	raidStructure->raidType=0;
    raidStructure->numOfDisks=0;
    raidStructure->numOfBlocks=0;
    raidStructure->isInitialized=0;
  	for(uint i=0;i<cnt;i++) {
          write_raid_info(disks[i].diskID);
          disks[i].diskID=0;
          disks[i].valid=0;
          disks[i].pair=0;
  	}
    releasesleep(lock);
  	return 0;
}