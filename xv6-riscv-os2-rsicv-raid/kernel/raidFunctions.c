#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

const int diskNum = 7;
const int diskSize = 1<<27;
const int blockSize = 1024;
const int disk_block_num = (diskSize/blockSize);

typedef struct {
    int (*read_raidType)(int, uchar*); // Pokazivač na funkciju
    int (*write_raidType)(int,uchar*);
    int (*disk_repair_raidType)(int);
    int (*disk_fail_raidType)(int);
    int block_num;
    int block_size;
    int rt;
    int* diskHealth;
    struct spinlock lock;

} RaidType;
RaidType currentRaid;
int loaded = 0;
void retriveCurrentRaid()
 {
    printf("pozvano za ucitavannnnnnje\n");
    uchar * data = kalloc();
    int blkNum = diskSize / blockSize -1 ;

    for(int i = 1; i<= diskNum; i++)
      {
      		read_block(i,blkNum, data);
                if(data[0]!=8)
                  	break;
      }
    initialize_raid_array(data[0]);
    for(int i = 1; i <= diskNum; i ++)
        currentRaid.diskHealth[i] = data[i];


    printf("ucitano\n");
 }

void saveRaidType() {
  uchar * data = kalloc();
  data[0] = currentRaid.rt;

  for(int i = 1; i <= diskNum; i ++)
        data[i] = currentRaid.diskHealth[i];
  int blkNum = diskSize/blockSize - 1;

  uchar * faulty = kalloc();
  faulty[0] = 8;
  for(int i = 1; i<=diskNum; i++){
    	if(currentRaid.diskHealth[i] == 1)
          write_block(i,blkNum, data);
        else
          write_block(i,blkNum, faulty);
        }

   kfree(data);
   kfree(faulty);
  printf("sacuvano\n");
}






int checkDiskHealth(int num)
{
    return currentRaid.diskHealth[num];
}

void setDiskHealth(int num)
{
  currentRaid.diskHealth[num] = 1;
}

void resetDiskHealth(int num)
{
  currentRaid.diskHealth[num] = 0;
}

void lock()
{
  	acquire(&currentRaid.lock);
}

void unlock()
{
 	release(&currentRaid.lock);
}


int min(int a, int b)
{
  if(a<b) return a;
  return b;
}

void xorData(uchar* diskx, uchar* parity)
{
  for(int i = 0; i< currentRaid.block_size; i ++)
    	*(parity+i) = *(parity+i) ^ *(diskx+i);

}
 //copy from blk2 to blk1
void copyData(uchar* blk1, uchar* blk2)
{
  	for(int i = 0; i< currentRaid.block_num; i ++)
          *(blk1 + i ) = *(blk2 + i);
}

int retriveRegularBlockRaid4_5(int diskCurr, int diskParity, int blkNum, uchar* data, int opt)
{

  	if(opt == 1){
	for(int i = 0; i<= diskNum; i++)
    	{
    	if(i == diskCurr) continue;
        if(checkDiskHealth(i) == 0) return -1;
    	}
    }

  read_block(diskParity, blkNum, data);
  uchar* temp = kalloc();

  for(int i = diskNum ; i!= 0; i--)
    {
    	if(i == diskParity) continue;
    	if( i == diskCurr) continue;
    	read_block(i, blkNum, temp);
    	xorData(temp, data);
    }
    kfree(temp);

    return 0;
}



int retriveRegularDiskRaid4_5(int diskN){
int diskParity = diskNum;
	uchar* data = kalloc();
    int status = retriveRegularBlockRaid4_5(diskN, diskParity, 0, data, 1);  //check for 1. block
    if(status == -1) return status;

    write_block(diskN, 0, data);
   	for(int i =1; i< 512;i++)
       {
       	 retriveRegularBlockRaid4_5(diskN, diskParity, i, data, 0);
         write_block(diskN, i , data);
       }


   kfree(data);
   return 0;

}

int retriveParityBlockRaid4(int blkNum, uchar* data, int opt)
{
  if(opt == 1){  //check if other disks are fine
	for(int i = 1; i< diskNum; i++)
    	{
        if(checkDiskHealth(i) == 0) return -1;
    	}
    }

   uchar* temp = kalloc();
   read_block(diskNum - 1, blkNum, temp);

   for(int i = 0; i<currentRaid.block_size;i ++)
     	*(data + i) = 0;

  for(int i = diskNum-1 ; i!= 0; i--)
    {
    	read_block(i, blkNum, temp);
    	xorData(temp, data);
    }

    kfree(temp);

    return 0;
}

int retriveParityDiskRaid4(int diskN)
{
  uchar* data = kalloc();
    if(retriveParityBlockRaid4(0,data, 1) == -1)  return -1;

    write_block(diskN,0,data);
    for(int i =1; i< 512; i ++)
      {
      	retriveParityBlockRaid4(i,data,0);
        write_block(diskN, i, data);
      }


    return 0;
}





int repairDiskRaid1(int diskRight, int diskFaulty)
{
  uchar* data = kalloc();
  //for(int i = 0; i < currentRaid.block_num; i ++){
  for(int i = 0; i < 512 ; i ++){
    read_block(diskRight,i,data);
    write_block(diskFaulty,i,data);
  }
  kfree(data);


  return 0;
 }



int raid0_read(int blkn, uchar* data) {
  int diskN =( blkn % diskNum) +1;
  if(checkDiskHealth(diskN) == 0) return -1;
  int blkNum  = blkn / diskNum;
  read_block(diskN, blkNum, data);

  return 0;
}

int raid1_read(int blkn, uchar* data) {
  int diskN = -1;
  for(int i = 1; i<= diskNum; i++){
    if(currentRaid.diskHealth[i] == 1){
      diskN = i;
      break;
    }
  }

  if(diskN == -1) return -1;

   read_block(diskN,blkn,data);
   return 0;
}
int raid0_1_read(int blkn, uchar* data) {
  int diskp = (blkn% (diskNum/2)) + 1;
  int diskc = diskp + diskNum/2;



  if(currentRaid.diskHealth[diskp]==0 && currentRaid.diskHealth[diskc] == 0)
    return -1;

  int blkNum  = blkn / (diskNum/2);

  if(currentRaid.diskHealth[diskp] != 0){   //if disc is healthy, read from it
    read_block(diskp,blkNum,data);

    return 0;
    }

  if(currentRaid.diskHealth[diskc] != 0)
    read_block(diskc,blkNum,data);




  return 0;
}
int raid4_read(int blkn, uchar* data) {
  int diskCurr = (blkn% (diskNum-1))+1;
  int blkNum = (blkn / (diskNum -1));
  int diskParity = diskNum;

  if(checkDiskHealth(diskCurr) == 1)
    {
    	read_block(diskCurr, blkNum, data);
        return 0;
    }


	return retriveRegularBlockRaid4_5(diskCurr, diskParity, blkNum, data, 1);


}
int raid5_read(int blkn, uchar* data) {


     int blkNum = blkn / (diskNum -1);
     int diskParity = blkNum % diskNum + 1;

     int nblkNum = blkn % (diskNum * (diskNum -1 ));
     int diskCurr = (nblkNum / (diskNum) + 1 + nblkNum)%diskNum + 1;

     if(checkDiskHealth(diskCurr) == 1){
       		read_block(diskCurr,blkNum,data);
            return 0;
       }

     return retriveRegularBlockRaid4_5(diskCurr,diskParity,blkNum,data,1);

}

void test_lock() {
    acquire(&currentRaid.lock);
    printf("Lock acquired\n");
    release(&currentRaid.lock);
    printf("Lock released\n");
}


int raid0_write(int blkn, uchar* data) {

  int diskN =( blkn % diskNum) +1;
  if(checkDiskHealth(diskN) == 0)
        return -1;

  int blkNum  = blkn / diskNum;
  write_block(diskN, blkNum, data);

  return 0;
}

int raid1_write(int blkn, uchar* data) {
  int diskN = -1;
  for(int i = 1; i<= diskNum; i++){
    if(currentRaid.diskHealth[i] == 1){
      diskN = i;
      write_block(diskN,blkn,data);
    }
  }
  return min(diskN, 0);

 }

int raid0_1_write(int blkn, uchar* data) {

  int diskp = (blkn% (diskNum/2)) + 1;
  int diskc = diskp + diskNum/2;

  if(currentRaid.diskHealth[diskp]==0 && currentRaid.diskHealth[diskc] == 0)
    return -1;

  int blkNum  = blkn / (diskNum/2);

  if(currentRaid.diskHealth[diskp] != 0)   //if disc is healthy, write on it
    write_block(diskp,blkNum,data);

  if(currentRaid.diskHealth[diskc] != 0)
    write_block(diskc,blkNum,data);

  return 0;
}

int writeRaid4_5(int diskCurr, int diskParity, int blkNum, uchar* data)
{
  if(checkDiskHealth(diskCurr) == -1 && checkDiskHealth(diskParity) == -1) return -1;

  if(checkDiskHealth(diskParity) == 0)
    {
     write_block(diskCurr,blkNum,data);
     return 0;
    }

    uchar * blockx = kalloc();
    uchar * parity = kalloc();
    read_block(diskParity,blkNum, parity);

    if(checkDiskHealth(diskCurr) == 1)
    	read_block(diskCurr, blkNum, blockx);

    else
      {
      	int status = retriveRegularBlockRaid4_5(diskCurr, diskParity, blkNum, blockx, 1);
        if(status <  0) return -1;
      }


    xorData(blockx, parity);
    xorData(data, parity);

    write_block(diskParity, blkNum, parity);

    if(checkDiskHealth(diskCurr) == 1)
    	write_block(diskCurr, blkNum, data);

    kfree(blockx);
    kfree(parity);
    return 0;
}

int raid4_write(int blkn, uchar* data) {


  int diskCurr = (blkn% (diskNum-1))+1;
  int blkNum = (blkn / (diskNum -1));
  int diskParity = diskNum;

  return writeRaid4_5(diskCurr,diskParity,blkNum,data);



}
int raid5_write(int blkn, uchar* data) {
  int blkNum = blkn / (diskNum -1);
  int diskParity = blkNum % diskNum + 1;
  int nblkNum = blkn % (diskNum * (diskNum -1 ));
  int diskCurr = (nblkNum / (diskNum) + 1 + nblkNum)%diskNum + 1;

  return writeRaid4_5(diskCurr,diskParity,blkNum,data);

}


int disk_repair_raid0(int diskN)
{
  resetDiskHealth(diskN);
  return 0;
}


int disk_repair_raid1(int diskFaulty){

  for(int i = 1; i<= diskNum;i ++)
    printf("%d ", currentRaid.diskHealth[i]);
  int diskRight = -1;
  for(int i = 1; i<= diskNum; i++){
    if(currentRaid.diskHealth[i] == 1){
      diskRight = i;
      break;
    }
  }
  if(diskRight == -1) return -1;
  return repairDiskRaid1(diskRight,diskFaulty);
}


int disk_repair_raid0_1(int diskN){
  int disk1 = diskN;
  int disk2 = diskN + diskNum / 2;
  if(disk2 > diskNum)
    disk2 = disk1 - diskNum / 2;

  printf("disk1 , disk2 %d %d\n", disk1, disk2);

  if(checkDiskHealth(disk2) == 0)
    return -1;

  return repairDiskRaid1(disk2, disk1);

}



int disk_repair_raid4(int diskN){

  if(diskN != diskNum)
  	 return retriveRegularDiskRaid4_5(diskN);

  return retriveParityDiskRaid4(diskN);


}
int disk_repair_raid5(int diskN){
  return retriveRegularDiskRaid4_5(diskN);
}

int disk_fail_raid0(int diskN)
{
  resetDiskHealth(diskN);
  return 0;
}

int disk_fail_raid1(int diskN){
    printf("disk failed : %d\n", diskN);

    currentRaid.diskHealth[diskN] = 0;
    return 0;
}
int disk_fail_raid0_1(int diskN){
  printf("disk failed : %d\n", diskN);
  currentRaid.diskHealth[diskN] = 0;
  return 0;

}
int disk_fail_raid4(int diskN){
  printf("disk failed : %d\n", diskN);
  currentRaid.diskHealth[diskN] = 0;
  return 0;}


int disk_fail_raid5(int diskN){
  printf("disk failed : %d\n", diskN);
  currentRaid.diskHealth[diskN] = 0;
  return 0;
}


#define NUM_RAID_LEVELS 8
static int (*read_array[NUM_RAID_LEVELS])();
static int (*write_array[NUM_RAID_LEVELS])();
static int (*disk_repair_array[NUM_RAID_LEVELS])();
static int (*disk_fail_array[NUM_RAID_LEVELS])();

static int blockNum[NUM_RAID_LEVELS];

void initialize_raid_array(int rt)
{
  initlock(&currentRaid.lock, "raidlock");

  read_array[0] = &raid0_read;
  read_array[1] = &raid1_read;
  read_array[2] = &raid0_1_read;
  read_array[3] = &raid4_read;
  read_array[4] = &raid5_read;

  write_array[0] = &raid0_write;
  write_array[1] = &raid1_write;
  write_array[2] = &raid0_1_write;
  write_array[3] = &raid4_write;
  write_array[4] = &raid5_write;

  disk_repair_array[0]= &disk_repair_raid0;
  disk_repair_array[1]= &disk_repair_raid1;
  disk_repair_array[2]= &disk_repair_raid0_1;
  disk_repair_array[3]= &disk_repair_raid4;
  disk_repair_array[4]= &disk_repair_raid5;

  disk_fail_array[0]= &disk_fail_raid0;
  disk_fail_array[1]= &disk_fail_raid1;
  disk_fail_array[2]= &disk_fail_raid0_1;
  disk_fail_array[3]= &disk_fail_raid4;
  disk_fail_array[4]= &disk_fail_raid5;

  blockNum[0] = (diskSize / blockSize) * diskNum - diskNum;
  blockNum[1] = diskSize / blockSize - 1;
  blockNum[2] = (diskSize / blockSize) * (diskNum/2) - diskNum/2;
  blockNum[3] = (diskSize / blockSize) * (diskNum - 1) - (diskNum - 1);
  blockNum[4] = blockNum[3];


  currentRaid.read_raidType = read_array[rt];
  currentRaid.write_raidType = write_array[rt];
  currentRaid.disk_fail_raidType = disk_fail_array[rt];
  currentRaid.disk_repair_raidType = disk_repair_array[rt];
  currentRaid.block_num = blockNum[rt];
  currentRaid.block_size = blockSize;
  currentRaid.rt = rt;
  currentRaid.diskHealth = kalloc();
  loaded = 1;
  for(int i =0;i <diskNum+1;i++)
    currentRaid.diskHealth[i] = 1;
}

int start_read(int blkn, uchar* data)
{

  	if(loaded == 0)
          retriveCurrentRaid();
  	return currentRaid.read_raidType(blkn,data);
}

int start_write(int blkn, uchar* data)
{
  if(loaded == 0)
          retriveCurrentRaid();
  	return currentRaid.write_raidType(blkn,data);
}

int start_disk_fail(int diskN)
{
    if(loaded == 0)
          retriveCurrentRaid();
    int status = 0;
    lock();
    status = currentRaid.disk_fail_raidType(diskN);
    unlock();
    return status;


}

int start_disk_repair(int diskN)
{
  if(loaded == 0)
          retriveCurrentRaid();

      if(currentRaid.diskHealth[diskN]==1) return 0;
      int status = 0;
      //lock();
      status = currentRaid.disk_repair_raidType(diskN);
	  //unlock();

      lock();
      if(status==0)
       	 setDiskHealth(diskN);
      unlock();
      return status;

}


void send_large_value(uint64 location, int large_value)
{
  char buffer[4];
  buffer[0] = (large_value >> 0) & 0xFF;  // Najnizi
  buffer[1] = (large_value >> 8) & 0xFF;
  buffer[2] = (large_value >> 16) & 0xFF;
  buffer[3] = (large_value >> 24) & 0xFF; // Najvisi
  copyout(myproc()->pagetable, location, buffer, sizeof(buffer));
}

int get_block_num_current_raid()
{
  if(loaded == 0)
          retriveCurrentRaid();
  return currentRaid.block_num;
}

int get_block_size()
{
    return currentRaid.block_size;
}

int get_num_disks()
{
  return diskNum;
}