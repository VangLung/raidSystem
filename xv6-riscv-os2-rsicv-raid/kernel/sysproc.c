#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"




uint64
sys_exit(void)
{
  int n;
  argint(0, &n);

  saveRaidType();
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}



uint64
sys_init_raid(void)
{
  int rt;
  argint(0,&rt);

  if(rt < 0 || rt > 6)
    return -1;

  initialize_raid_array(rt);

  return 0;
}

uint64
sys_read_raid(void)
{
  int blkn;
  uint64 addr;
  argint(0,&blkn);
  argaddr(1,&addr);
  uchar* data = kalloc();
  int status = start_read(blkn, data);
  copyout(myproc()->pagetable, addr, (char*) data, get_block_size());
  return status;
}

uint64
sys_write_raid(void)
{
  int blkn;
  uint64 address;

  uchar* data = kalloc();
  argint(0,&blkn);
  argaddr(1,&address);

  copyin(myproc()->pagetable, (char*) data, address, get_block_size());
  return start_write(blkn,data);



}



uint64
sys_disk_fail_raid(void)
{
  int diskN;
  argint(0,&diskN);
  if(diskN <= 0 || diskN > get_num_disks()) return -1;
  start_disk_fail(diskN);
  return 0;
}

uint64
sys_disk_repaired_raid(void)
{
  int diskN;
  argint(0,&diskN);
  if(diskN <= 0 || diskN > get_num_disks()) return -1;
  start_disk_repair(diskN);
  return 0;
}

uint64 sys_info_raid(void) {
  uint64 blkn;
  uint64 blks;
  uint64 diskn;
  argaddr(0, &blkn);
  argaddr(1,&blks);
  argaddr(2,&diskn);
  int large_value = get_block_num_current_raid();
  send_large_value(blkn, large_value);   //numbers are too big, so we need to send them as an array of chars
  large_value = get_block_size();
  send_large_value(blks,large_value);
  large_value = get_num_disks();
  send_large_value(diskn,large_value);
  return 0;

}

uint64
sys_destroy_raid(void)
{
  return 0;
}
