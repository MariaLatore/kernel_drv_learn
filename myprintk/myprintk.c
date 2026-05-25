#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

static int __init ratelimit_test_init(void)
{
	int i;

	for (i = 0; i < 20; i++)
		pr_warn_ratelimited("ratelimit_test: message %d\n", i);

	return 0;
}

static void __exit ratelimit_test_exit(void)
{
	pr_info("ratelimit_test: exit\n");
}

module_init(ratelimit_test_init);
module_exit(ratelimit_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("test");
MODULE_DESCRIPTION("printk_ratelimit test");
