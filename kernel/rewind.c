#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/signal.h>
#include <linux/mm_rewind.h>
#include <asm/msr.h>

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/smp.h>

static void rewind_threads(void)
{
	struct task_struct *tsk;
	int err;

	if (current->rewind_nr_threads == get_nr_threads(current))
		return;

	err = rewind_de_thread(current);
	if (err) {
		printk(KERN_INFO "REWIND(RW): de thread error %d\n", err);
	}

	list_for_each_entry(tsk, &current->children, sibling) {
		printk(KERN_INFO "REWIND(RW): Kill pid %d\n", tsk->pid);
		send_sig (SIGKILL, tsk, 0);
	}
}


SYSCALL_DEFINE1(checkpoint, unsigned long __user, num)	// syscall(548)
{
	struct task_struct *tsk = current;
	unsigned long long tmp;

	printk(KERN_INFO "REWIND(CHK): Current task(%d,%d) is rewindable!\n", current->pid, current->tgid);

	tmp = rdtsc();

	tsk->rewind_cp = 0;
	tsk->rewind_cnt = 1;
	tsk->rewind_pte_cnt = 0;
	tsk->mm->rewindable = 1;

	tsk->rewind_nr_threads = get_nr_threads(tsk);

	copy_pgt(tsk->mm, DO_CHECKPOINT);

	tmp = rdtsc() - tmp;

	printk(KERN_INFO "REWIND(CHK): Takes %llu ns, pte_cnt %lu\n", tmp, tsk->rewind_pte_cnt);

	tsk->rewind_vma_reuse = 0;
	tsk->rewind_vma_alloc = 0;
	
	return 0;
}

SYSCALL_DEFINE1(rewind, unsigned long __user, num)	// syscall(549)
{
	struct task_struct *tsk = current;
	unsigned long long tmp, tmpt;

	printk(KERN_INFO "REWIND(RW): REWIND start time %llu\n", ktime_get_real_ns());
	
	rewind_threads();
	
	tsk->rewind_pte = 0;
	tsk->rewind_clear = 0;
	tsk->rewind_unmap = 0;
	tsk->rewind_flush = 0;
	tsk->rewind_page_cnt = 0;
	tsk->rewind_pte_cnt = 0;
	tsk->rewind_total_vma = 0;
	tsk->rewind_unused_vma = 0;
	tsk->rewind_vma = 0;

	tsk->rewind_page_reuse = 0;
	tsk->rewind_reusable_size = 0;

	tsk->rewind_page_erase_cnt = 0;
	tsk->rewind_page_cow_cnt = 0;

	//printk(KERN_INFO "REWIND(RW): Test rewind() with one parameter - %lu\n", num);
	//printk(KERN_INFO "REWIND(RW): rewindable?: %u\n", tsk->rewindable);
	tsk->rewind_cnt++;

	tmpt = ktime_get_ns();
	tmp = rdtsc();
	copy_pgt(tsk->mm, DO_REWIND); // return type int (have to if state)
	tmp = rdtsc()-tmp;
	tmpt = ktime_get_ns() - tmpt;

	//printk(KERN_INFO "REWIND(RW): End time=%llu, Total_time=%llu, VMA_Unmap=%llu, PTE_copy=%llu, Page_clear=%llu, Flush=%llu, ktime=%llu\n", ktime_get_real_ns(), tmp, tsk->rewind_unmap, tsk->rewind_pte, tsk->rewind_clear, tsk->rewind_flush, tmpt);
	//printk(KERN_INFO "REWIND(RW): Clear_page=%lu, Access_pte=%lu Erase_page=%lu Cow_page=%lu\n", tsk->rewind_page_cnt, tsk->rewind_pte_cnt, tsk->rewind_page_erase_cnt, tsk->rewind_page_cow_cnt);
	//printk(KERN_INFO "REWIND(vma): Total=%lu, rewinds=%lu, unused_set=%lu\n", tsk->rewind_total_vma, tsk->rewind_vma, tsk->rewind_unused_vma);
	//printk(KERN_INFO "REIWND(ptw): PGD=%llu, P4D=%llu, PUD=%llu, PMD=%llu, PTE=%llu\n",
	printk(KERN_INFO "REWIND(alloc_vma): vma_alloc=%lu, vma_reuse=%lu\n", tsk->rewind_vma_alloc, tsk->rewind_vma_reuse);
	printk(KERN_INFO "REWIND(reuse): %lu(current) / %lu(next)", tsk->rewind_reused_size, tsk->rewind_reusable_size);
	tsk->rewind_vma_reuse = 0;
        tsk->rewind_vma_alloc = 0;
	tsk->rewind_reused_page = 0;
	tsk->rewind_reused_size = 0;

	return 0;
}

SYSCALL_DEFINE0(rewindable)	// syscall(550)
{
	printk(KERN_INFO "REWIND(able): Test rewind_parent become 1\n");
	current->rewind_parent = 1;
	printk(KERN_INFO "REWIND(able): rewind_parent = %u, rewindable = %u\n", current->rewind_parent, current->rewindable);
	
	return 0;
}

SYSCALL_DEFINE0(rewind_dbg)	// syscall(551)
{
	printk(KERN_INFO "REWIND(TIME): Current time = %llu, Total pf time = %llu\n", ktime_get_real_ns(), current->rewind_time);
	printk(KERN_INFO "REWIND(alloc_vma): vma_alloc=%lu, vma_reuse=%lu\n", current->rewind_vma_alloc, current->rewind_vma_reuse);
	current->rewind_vma_reuse = 0;
        current->rewind_vma_alloc = 0;
	//printk(KERN_INFO "REWIND(TIME): PF LIST TOTAL=%lu, ANON_READ=%lu, ANON_WRITE=%lu,
	//FILE_READ=%lu, FILE_WRITE=%lu, FILE_SHARE=%lu, WP_COW=%lu, WP_REUSE=%lu, WP_FILE_REUSE=%lu\n",
	//current->rewind_pf_cnt, current->rewind_pf_list[0], current->rewind_pf_list[1],
	//current->rewind_pf_list[2], current->rewind_pf_list[3], current->rewind_pf_list[4],
	//current->rewind_pf_list[5], current->rewind_pf_list[6], current->rewind_pf_list[7]);
	return 0;
}

SYSCALL_DEFINE0(fork_dbg)	// syscall(552)
{
	current->child_print = 1;
	return 0;
}
