#include <linux/init.h> /* Needed for macros */
#include <linux/module.h> /* Needed by all modules */
#include <linux/printk.h> /* Needed for pr_info() */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bpb");
MODULE_DESCRIPTION("A sample driver eith a GPL license");

static int __init init_hello(void)
{
	pr_info("Hello, world\n");
	return 0;
}

static void __exit cleanup_hello(void)
{
	pr_info("Goodbye, world\n");
}
module_init(init_hello);
module_exit(cleanup_hello);
