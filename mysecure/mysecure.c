#include <linux/kernel.h>
#include <linux/array_size.h>
#include <linux/lsm_hooks.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/security.h>
#include <linux/fs.h>

static struct lsm_id mylsmid = {
  .name = "zxylsm",
};

static bool my_task_is_allowed(struct task_struct *task, int mask)
{
    pr_info("zxylsm: check pid=%d mask=0x%x\n", task->pid, mask);
    return true;
}

//Hook function for file permissions
static int my_lsm_inode_permission(struct inode *inode, int mask){
	//Check if the current process has permission to access the file
	if(!my_task_is_allowed(current,mask)){
		pr_info("LSM: Access denied for PID %d to inode %lu\n", current->pid, inode->i_ino);
		return -EACCES; //Access denied
	}
	return 0; //Access granted
}

//Hook function for file creation
static int my_lsm_inode_create(struct inode *dir, struct dentry *dentry, umode_t mode){
	//Check if the current process is allowed to create a file in the directory
	if(!my_task_is_allowed(current, MAY_WRITE)){
		pr_info("LSM: Creation of file denied for PID %d in directory %lu\n", current->pid, dir->i_ino);
		return -EACCES; //Creation denied
	}
	pr_info("zxylsm: inode_create pid=%d dir=%lu\n", current->pid, dir->i_ino);
	return 0; //Creation allowed
}

//Hook function for file deletion
static int my_lsm_inode_unlink(struct inode *dir, struct dentry *dentry){
	//Check if the current process is allowed to delete the file
	if(!my_task_is_allowed(current, MAY_WRITE)){
		pr_info("LSM: Deletion of file denied for PID %d in directory %lu\n", current->pid, dir->i_ino);
		return -EACCES; //Deletion denied
	}
	pr_info("zxtlsm: inode_unlink pid=%d dir=%lu\n", current->pid, dir->i_ino);
	return 0; //Deletion allowed
}

//Hook functions registration
static struct security_hook_list my_lsm_hooks[] = {
	LSM_HOOK_INIT(inode_permission, my_lsm_inode_permission),
	LSM_HOOK_INIT(inode_create, my_lsm_inode_create),
	LSM_HOOK_INIT(inode_unlink, my_lsm_inode_unlink),
	//Add more hooks as needed
};

//LSM initialization
static int __init my_lsm_init(void){
	pr_info("zxylsm: Initializing zxyLSM\n");
	//Register the LSM hooks
	security_add_hooks(my_lsm_hooks, ARRAY_SIZE(my_lsm_hooks),&mylsmid);
	return 0;
}

/*
//LSM cleanup
static void __exit my_lsm_exit(void){
	pr_info("LSM: Cleaning up LSM\n");
}
*/

DEFINE_LSM(zxylsm) = {
    .name = "zxylsm",
    .init = my_lsm_init,
};

/*
module_init(my_lsm_init);
module_exit(my_lsm_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("BPB");
MODULE_DESCRIPTION("A simple Linux Security Module");
*/
