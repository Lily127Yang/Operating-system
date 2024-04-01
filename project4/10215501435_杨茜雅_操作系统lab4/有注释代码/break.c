/* The MINIX model of memory allocation reserves a fixed amount of memory for
 * the combined text, data, and stack segments.  The amount used for a child
 * process created by FORK is the same as the parent had.  If the child does
 * an EXEC later, the new size is taken from the header of the file EXEC'ed.
 *
 * The layout in memory consists of the text segment, followed by the data
 * segment, followed by a gap (unused memory), followed by the stack segment.
 * The data segment grows upward and the stack grows downward, so each can
 * take memory from the gap.  If they meet, the process must be killed.  The
 * procedures in this file deal with the growth of the data and stack segments.
 *
 * The entry points into this file are:
 *   do_brk:	  BRK/SBRK system calls to grow or shrink the data segment
 *   adjust:	  see if a proposed segment adjustment is allowed
 *   size_ok:	  see if the segment sizes are feasible (i86 only)
 */

#include "pm.h"
#include <signal.h>
#include "mproc.h"
#include "param.h"
#include <lib.h>

#define DATA_CHANGED       1	/* flag value when data segment size 
*/
#define STACK_CHANGED      2	/* flag value when stack size changed */

/*===========================================================================*
 *				do_brk  				     *
 *===========================================================================*/
PUBLIC int do_brk()
{
/* Perform the brk(addr) system call.
 *
 * The call is complicated by the fact that on some machines (e.g., 8088),
 * the stack pointer can grow beyond the base of the stack segment without
 * anybody noticing it.
 * The parameter, 'addr' is the new virtual address in D space.
 */

  register struct mproc *rmp;//指向当前进程的结构体指针
  int r;
  vir_bytes v, new_sp;// 新虚拟地址,  新栈顶地址
  vir_clicks new_clicks;// 新的数据段长度

  rmp = mp; /*current process*/
  v = (vir_bytes) m_in.addr;// 获取参数 addr，即新的虚拟地址
  new_clicks = (vir_clicks) ( ((long) v + CLICK_SIZE - 1) >> CLICK_SHIFT);// 将新虚拟地址转换为数据段长度（以clicks表示）
  if (new_clicks < rmp->mp_seg[D].mem_vir) { /* points to text, data, stack */
  //检查新的数据段长度是否小于当前进程的数据段起始位置，即新的虚拟地址指向了文本段、数据段或堆栈段
  //确实小于
	rmp->mp_reply.reply_ptr = (char *) -1;
	return(ENOMEM);//分配失败
  }
  new_clicks -= rmp->mp_seg[D].mem_vir;//新的数据段长度减去当前进程的数据段起始位置，以得到相对于数据段的新长度
  if ((r=get_stack_ptr(who_e, &new_sp)) != OK) /* ask kernel for sp value */
  	panic(__FILE__,"couldn't get stack pointer", r);
  // 调用adjust函数调整数据段的长度和栈顶地址
  r = adjust(rmp, new_clicks, new_sp);
  rmp->mp_reply.reply_ptr = (r == OK ? m_in.addr : (char *) -1);//// 返回新的地址或-1，表示分配失败
  return(r);			/* return new address or -1 */
}

/*===========================================================================*
 *				adjust  				     *
 *===========================================================================*/
 
PUBLIC int adjust(rmp, data_clicks, sp)
register struct mproc *rmp;	/* whose memory is being adjusted? */
vir_clicks data_clicks;		/* how big is data segment to become? */
vir_bytes sp;			/* new value of sp */
{
/* See if data and stack segments can coexist, adjusting them if need be.
 * Memory is never allocated or freed.  Instead it is added or removed from the
 * gap between data segment and stack segment.  If the gap size becomes
 * negative, the adjustment of data or stack fails and ENOMEM is returned.
 */

  register struct mem_map *mem_sp, *mem_dp;
  vir_clicks sp_click, gap_base, lower, old_clicks;
  int changed, r, ft, z;
  long base_of_stack, delta;	/* longs avoid certain problems */

  mem_dp = &rmp->mp_seg[D];	/* pointer to data segment map */
  mem_sp = &rmp->mp_seg[S];	/* pointer to stack segment map */
  changed = 0;			/* set when either segment changed */

  if (mem_sp->mem_len == 0) return(OK);	/* don't bother init */

  /* See if stack size has gone negative (i.e., sp too close to 0xFFFF...) */
  base_of_stack = (long) mem_sp->mem_vir + (long) mem_sp->mem_len;
  sp_click = sp >> CLICK_SHIFT;	/* click containing sp */
  if (sp_click >= base_of_stack) return(ENOMEM);	/* sp too high */

  /* Compute size of gap between stack and data segments. */
  delta = (long) mem_sp->mem_vir - (long) sp_click;
  lower = (delta > 0 ? sp_click : mem_sp->mem_vir);

  /* Add a safety margin for future stack growth. Impossible to do right. */
#define SAFETY_BYTES  (384 * sizeof(char *))
#define SAFETY_CLICKS ((SAFETY_BYTES + CLICK_SIZE - 1) / CLICK_SIZE)
  gap_base = mem_dp->mem_vir + data_clicks + SAFETY_CLICKS;
  if (lower < gap_base) {
  	z = allocate_new_mem(rmp, data_clicks, sp);
	if(z==ENOMEM) return(ENOMEM);
	return(OK);
  	}	/* data and stack collided */

  /* Update data length (but not data orgin) on behalf of brk() system call. */
  old_clicks = mem_dp->mem_len;
  if (data_clicks != mem_dp->mem_len) {
	mem_dp->mem_len = data_clicks;
	changed |= DATA_CHANGED;
  }

  /* Update stack length and origin due to change in stack pointer. */
  if (delta > 0) {
	mem_sp->mem_vir -= delta;
	mem_sp->mem_phys -= delta;
	mem_sp->mem_len += delta;
	changed |= STACK_CHANGED;
  }

  /* Do the new data and stack segment sizes fit in the address space? */
  ft = (rmp->mp_flags & SEPARATE);
#if (CHIP == INTEL && _WORD_SIZE == 2)
  r = size_ok(ft, rmp->mp_seg[T].mem_len, rmp->mp_seg[D].mem_len, 
       rmp->mp_seg[S].mem_len, rmp->mp_seg[D].mem_vir, rmp->mp_seg[S].mem_vir);
#else
  r = (rmp->mp_seg[D].mem_vir + rmp->mp_seg[D].mem_len > 
          rmp->mp_seg[S].mem_vir) ? ENOMEM : OK;
#endif
  if (r == OK) {
	int r2;
	if (changed && (r2=sys_newmap(rmp->mp_endpoint, rmp->mp_seg)) != OK)
  		panic(__FILE__,"couldn't sys_newmap in adjust", r2);
	return(OK);
  }

  /* New sizes don't fit or require too many page/segment registers. Restore.*/
  if (changed & DATA_CHANGED) mem_dp->mem_len = old_clicks;
  if (changed & STACK_CHANGED) {
	mem_sp->mem_vir += delta;
	mem_sp->mem_phys += delta;
	mem_sp->mem_len -= delta;
  }
  return(ENOMEM);
}

#if (CHIP == INTEL && _WORD_SIZE == 2)
/*===========================================================================*
 *				size_ok  				     *
 *===========================================================================*/
PUBLIC int size_ok(file_type, tc, dc, sc, dvir, s_vir)
int file_type;			/* SEPARATE or 0 */
vir_clicks tc;			/* text size in clicks */
vir_clicks dc;			/* data size in clicks */
vir_clicks sc;			/* stack size in clicks */
vir_clicks dvir;		/* virtual address for start of data seg */
vir_clicks s_vir;		/* virtual address for start of stack seg */
{
/* Check to see if the sizes are feasible and enough segmentation registers
 * exist.  On a machine with eight 8K pages, text, data, stack sizes of
 * (32K, 16K, 16K) will fit, but (33K, 17K, 13K) will not, even though the
 * former is bigger (64K) than the latter (63K).  Even on the 8088 this test
 * is needed, since the data and stack may not exceed 4096 clicks.
 * Note this is not used for 32-bit Intel Minix, the test is done in-line.
 */

  int pt, pd, ps;		/* segment sizes in pages */

  pt = ( (tc << CLICK_SHIFT) + PAGE_SIZE - 1)/PAGE_SIZE;
  pd = ( (dc << CLICK_SHIFT) + PAGE_SIZE - 1)/PAGE_SIZE;
  ps = ( (sc << CLICK_SHIFT) + PAGE_SIZE - 1)/PAGE_SIZE;

  if (file_type == SEPARATE) {
	if (pt > MAX_PAGES || pd + ps > MAX_PAGES) return(ENOMEM);
  } else {
	if (pt + pd + ps > MAX_PAGES) return(ENOMEM);
  }

  if (dvir + dc > s_vir) return(ENOMEM);

  return(OK);
}
#endif
/*===========================================================================*
 *				allocate_new_mem  				     *
 *===========================================================================*/
PUBLIC int allocate_new_mem(rmp, data_clicks, sp)
register struct mproc *rmp;
vir_clicks data_clicks;		/* how big is data segment to become? */
vir_bytes sp;		/* new value of sp */
{
  int changed, r, ft, d;
  long delta;
  vir_clicks sp_click;
  phys_clicks old_clicks;
  phys_clicks old_base;
  phys_clicks new_base;

  phys_bytes old_d_tran;
  phys_bytes new_d_tran;
  phys_bytes d_len;
  phys_bytes old_s_tran;
  phys_bytes new_s_tran;
  phys_bytes s_len;
  
  
  register struct mem_map *mem_sp, *mem_dp;
  mem_dp = &rmp->mp_seg[D];	/* pointer to data segment map */
  mem_sp = &rmp->mp_seg[S];	/* pointer to stack segment map */
  sp_click = (sp >> CLICK_SHIFT);	/* click containing sp */

  delta = (long) mem_sp->mem_vir - (long) sp_click;
// 保存原始数据段和堆栈段的物理地址和长度
  old_base = mem_dp->mem_phys;
  old_clicks = mem_sp->mem_vir + mem_sp->mem_len - mem_dp->mem_vir;
// 分配新的内存空间，大小为原始数据段和堆栈段的两倍
  new_base = alloc_mem(2*old_clicks);
  if(new_base == NO_MEM) {//检查内存分配是否成功，NO_MEM 是一个常量，表示内存分配失败
  	printf("no more space\n");//表示没有足够的空间分配新的内存
  	return(ENOMEM);
  }else{//如果内存分配成功
  	printf("alloc mem in addr %d\n",new_base);//打印分配的新内存空间的起始地址
  	}
// 计算需要拷贝的原始数据段和堆栈段的起始地址和长度
  old_d_tran = mem_dp->mem_phys << CLICK_SHIFT;
  new_d_tran = new_base << CLICK_SHIFT;
  d_len = mem_dp->mem_len << CLICK_SHIFT;
  old_s_tran = mem_sp->mem_phys << CLICK_SHIFT;
  new_s_tran = (new_base + 2*old_clicks - mem_sp->mem_len) << CLICK_SHIFT;
  s_len = mem_sp->mem_len << CLICK_SHIFT;
  // 将原始数据段拷贝至新的数据段底部
  d = sys_abscopy(old_d_tran, new_d_tran, d_len);
  if(d < 0)
  	panic(__FILE__,"can't copy data segment in allocate_new_mem", d);
  // 将原始堆栈段拷贝至新的堆栈段顶部
  d = sys_abscopy(old_s_tran, new_s_tran, s_len);
  if(d < 0)
  	panic(__FILE__,"can't copy stack segment in allocate_new_mem", d);
  // 更新进程控制块中数据段和堆栈段的信息
  mem_dp->mem_phys = new_base;
  mem_sp->mem_phys = new_base + 2*old_clicks - (mem_sp->mem_vir+mem_sp->mem_len-sp_click);
  mem_dp->mem_len = data_clicks;
  changed |= DATA_CHANGED;
  mem_sp->mem_len = mem_sp->mem_vir+mem_sp->mem_len-sp_click;
  changed |= STACK_CHANGED;
  mem_sp->mem_vir = mem_dp->mem_vir + 2*old_clicks - mem_sp->mem_len;
  // 检查新的数据段和堆栈段的大小是否适合进程的地址空间
  /* Do the new data and stack segment sizes fit in the address space? */
  ft = (rmp->mp_flags & SEPARATE);
#if (CHIP == INTEL && _WORD_SIZE == 2)
  r = size_ok(ft, rmp->mp_seg[T].mem_len, rmp->mp_seg[D].mem_len, 
       rmp->mp_seg[S].mem_len, rmp->mp_seg[D].mem_vir, rmp->mp_seg[S].mem_vir);
#else
  r = (rmp->mp_seg[D].mem_vir + rmp->mp_seg[D].mem_len > 
          rmp->mp_seg[S].mem_vir) ? ENOMEM : OK;
#endif
// 如果新的大小适合，更新内存映射表
  if (r == OK) {
	int r2;
	if (changed && (r2=sys_newmap(rmp->mp_endpoint, rmp->mp_seg)) != OK)
  		panic(__FILE__,"couldn't sys_newmap in allocate_new_mem", r2);
	return(OK);
  }
/* 新的大小不适合或需要过多的页/段寄存器。恢复原始状态。*/
 /* New sizes don't fit or require too many page/segment registers. Restore.*/
  if (changed & DATA_CHANGED) mem_dp->mem_len = old_clicks;
  if (changed & STACK_CHANGED) {
	mem_sp->mem_vir += delta;
	mem_sp->mem_phys += delta;
	mem_sp->mem_len -= delta;
  }
// 释放之前分配的新内存空间
free_mem(old_base, old_clicks);

}

