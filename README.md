# RAID Support for xv6 Operating System

## Overview

This project implements a **RAID (Redundant Array of Independent Disks) management subsystem** for the **xv6 operating system** running on the **RISC-V architecture**.  
The system enables multiple physical disks to be combined into a single logical disk, with the goal of **improving performance**, **increasing reliability**, or both, depending on the selected RAID configuration.

The implementation integrates with xv6’s block device layer and supports **persistent RAID configuration**, **disk failure handling**, **automatic recovery**, and **safe concurrent access** in a multiprocess environment.

> **Academic context:**  
> This project was developed as part of an **academic course on Operating Systems 2**, with the purpose of exploring low-level storage systems, fault tolerance, and kernel-level synchronization.

---

## Supported RAID Levels

The system supports the following RAID structures:

- **RAID 0 (Striping)**  
  Improves performance by distributing blocks across multiple disks.

- **RAID 1 (Mirroring)**  
  Increases reliability by duplicating data on multiple disks.

- **RAID 0+1**  
  Combines striping and mirroring for both performance and fault tolerance.

- **RAID 4**  
  Block-level striping with a **dedicated parity disk**.

- **RAID 5**  
  Block-level striping with **distributed parity** across all disks.

---

## System Calls API

The RAID subsystem is exposed through the following system calls:

```c
enum RAID_TYPE { RAID0, RAID1, RAID0_1, RAID4, RAID5 };

int init_raid(enum RAID_TYPE raid);
int read_raid(int blkn, uchar* data);
int write_raid(int blkn, uchar* data);
int disk_fail_raid(int diskn);
int disk_repaired_raid(int diskn);
int info_raid(uint *blkn, uint *blks, uint *diskn);
int destroy_raid();
```
---
## Core Features
- Initialization of RAID structures with persistent storage
- Block-level read and write operations
- Disk failure and repair handling
- Continued operation in the presence of disk failures (when supported by RAID level)
- Automatic data recovery on repaired disks
- Retrieval of RAID configuration information
- Clean RAID destruction and resource cleanup

---
## Concurrency and Safety
- All system calls are thread-safe
- Full support for multi-process environments (fork)
- Mutual exclusion for disk access
- Safe handling of concurrent read/write operations

---
## Disk Access Interface

Low-level disk access is performed using the following functions:
```c
void write_block(int diskn, int blkn, uchar* data);
void read_block(int diskn, int blkn, uchar* data);
```
---
## Characteristics
- Block-based transfers (one block per operation)
- Blocking I/O operations
- Interrupts enabled during transfers
---
## Persistence
- RAID configuration and metadata are stored on disk
- Data remains intact across system restarts
- Existing RAID structures are automatically detected on boot
