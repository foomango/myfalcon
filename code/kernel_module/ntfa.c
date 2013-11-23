/*
 *  ntfa.c -- Communication with the hypervisor via kernel virtual memory
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ntfa.h>
#include <linux/proc_fs.h>

struct proc_dir_entry *ntfa_entry;

// ntfa_count is an unsigned long that is initialized by the hypervisor to be
// the VM's current generation. At initialization, this module copies the 
// value into module-local storage and returns that value on read.
//
// On write, it updates the value in ntfa_count 

static unsigned long generation;

static int
ntfa_write(struct file *file, const char __user *buffer,
	   unsigned long count, void *data) {
	   // Note: the data the user writes is totally ignored.
	   ntfa_count++;
	   return count;
}

// Return the lsb of ntfa_count
static int
ntfa_read(char *page, char **start, off_t off, int count, int *eof, void *data) {
	memcpy(&(page[off]), &generation, sizeof(generation));
	*eof = 1;
	return sizeof(generation);
}

static int __init init_ntfa(void) {
	ntfa_entry = create_proc_entry("ntfa", 0600, NULL);
	if (NULL == ntfa_entry) {
		return -ENOMEM;
	}
	generation = ntfa_count;
	ntfa_count = 0;
	ntfa_entry->data = NULL;
	ntfa_entry->read_proc = ntfa_read;
	ntfa_entry->write_proc = ntfa_write;
	return 0;
}

static void __exit exit_ntfa(void) {
	remove_proc_entry("ntfa", NULL);
}

module_init(init_ntfa);
module_exit(exit_ntfa);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Joshua B. Leners <leners@cs.utexas.edu>");
MODULE_DESCRIPTION("NTFA Virtual Memory Tickler");
