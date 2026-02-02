//
// Created by os on 12/27/24.
//
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "raid.h"
#include "proc.h"

uint64 sys_init_raid(void){
    int type;
    argint(0, &type);
    return init_raid(type);
}

uint64 sys_read_raid(void){
    int blkn=0;
    uint64 data=0;

    argint(0, &blkn);
    argaddr(1, &data);

    uchar* buffer=(uchar*)kalloc();
    int retVal=read_raid(blkn, buffer);
    copyout(myproc()->pagetable, data, (char*)buffer, BLOCK_SIZE);
    kfree(buffer);
    return retVal;
}

uint64 sys_write_raid(void){
    int blkn=0;
    uint64 data=0;

    argint(0, &blkn);
    argaddr(1, &data);
    uchar* buffer=(uchar*)kalloc();

    copyin(myproc()->pagetable, (char*)buffer, data, BLOCK_SIZE);
    int retVal = write_raid(blkn, buffer);
    kfree(buffer);
    return retVal;
}

uint64 sys_disk_fail_raid(void){
    int diskNo;
    argint(0, &diskNo);
    return disk_fail_raid(diskNo);
}

uint64 sys_disk_repaired_raid(void){
    int diskNo;
    argint(0, &diskNo);
    return disk_repaired_raid(diskNo);
}

uint64 sys_info_raid(void){
    uint64 blkn=0;
    uint64 blks=0;
    uint64 diskn=0;
    argaddr(0, &blkn);
    argaddr(1, &blks);
    argaddr(2, &diskn);

    uint* addr=(uint*)kalloc();
    uint* blknAddr=(uint*)addr;
    uint* blksAddr=(uint*)addr+1;
    uint* disknAddr=(uint*)blknAddr+2;
    //printf("\nPROCITAO SAM ARGUMENTE\n");
    int retVal = info_raid(blknAddr, blksAddr, disknAddr);

    copyout(myproc()->pagetable, blkn, (char*)blknAddr, sizeof(uint));
    copyout(myproc()->pagetable, blks, (char*)blksAddr, sizeof(uint));
    copyout(myproc()->pagetable, diskn, (char*)disknAddr, sizeof(uint));
    kfree(addr);
    return retVal;
}

uint64 sys_destroy_raid(void){
    return destroy_raid();
}