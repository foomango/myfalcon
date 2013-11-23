/*
 *  disksleep.c -- put any process reading or writing this file into
 *  uninterruptible sleep. 
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ntfa.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

struct proc_dir_entry *ntfa_entry;


static unsigned long generation;
static int sleep_time = 20 * 1000;

module_param(sleep_time, int, 0000);
MODULE_PARM_DESC(sleep_time, "Time to pause the disk for in milliseconds");


static int
ntfa_write(struct file *file, const char __user *buffer,
	   unsigned long count, void *data) {
	   // Note: the data the user writes is totally ignored
           msleep(sleep_time);
	   ntfa_count++;
	   return count;
}

// Return the lsb of ntfa_count
static int
ntfa_read(char *page, char **start, off_t off, int count, int *eof, void *data) {
        
	memcpy(&(page[off]), &generation, sizeof(generation));
        msleep(sleep_time);
	*eof = 1;
	return sizeof(generation);
}

static int __init init_ntfa(void) {
	ntfa_entry = create_proc_entry("badfd", 0666, NULL);
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
MODULE_DESCRIPTION("Create buggy fd to simulate wonky hardware");
