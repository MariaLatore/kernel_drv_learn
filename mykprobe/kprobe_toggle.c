#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kprobes.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>

static char *symbol = "do_sys_open";
module_param(symbol, charp, 0644);
MODULE_PARM_DESC(symbol, "Kernel symbol to probe");

static int probe_enabled = 1;
MODULE_PARM_DESC(probe_enabled, "Enable/disable the registered kprobe");

static struct kprobe kp;
static DEFINE_MUTEX(kp_lock);
static bool kp_registered;

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	pr_info("kprobe hit: %s\n", p->symbol_name);
	return 0;
}

static int set_probe_enabled(const char *val, const struct kernel_param *kp_param)
{
	int ret;
	int new_val;

	ret = kstrtoint(val, 0, &new_val);
	if (ret)
		return ret;

	if (new_val != 0 && new_val != 1)
		return -EINVAL;

	mutex_lock(&kp_lock);

	if (!kp_registered) {
		mutex_unlock(&kp_lock);
		return -EINVAL;
	}

	if (new_val && !probe_enabled) {
		enable_kprobe(&kp);
		probe_enabled = 1;
		pr_info("kprobe enabled\n");
	} else if (!new_val && probe_enabled) {
		disable_kprobe(&kp);
		probe_enabled = 0;
		pr_info("kprobe disabled\n");
	}

	mutex_unlock(&kp_lock);
	return 0;
}

static int get_probe_enabled(char *buffer, const struct kernel_param *kp_param)
{
	return scnprintf(buffer, PAGE_SIZE, "%d\n", probe_enabled);
}

static const struct kernel_param_ops probe_enabled_ops = {
	.set = set_probe_enabled,
	.get = get_probe_enabled,
};

module_param_cb(probe_enabled, &probe_enabled_ops, &probe_enabled, 0644);

static int __init kprobe_toggle_init(void)
{
	int ret;

	memset(&kp, 0, sizeof(kp));
	kp.symbol_name = symbol;
	kp.pre_handler = handler_pre;

	ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_err("register_kprobe failed for %s, ret=%d\n", symbol, ret);
		return ret;
	}

	kp_registered = true;
	pr_info("kprobe registered at %s\n", symbol);

	if (!probe_enabled) {
		disable_kprobe(&kp);
		pr_info("kprobe initially disabled\n");
	} else {
		pr_info("kprobe initially enabled\n");
	}

	return 0;
}

static void __exit kprobe_toggle_exit(void)
{
	mutex_lock(&kp_lock);
	if (kp_registered) {
		unregister_kprobe(&kp);
		kp_registered = false;
		pr_info("kprobe unregistered\n");
	}
	mutex_unlock(&kp_lock);
}

module_init(kprobe_toggle_init);
module_exit(kprobe_toggle_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("example");
MODULE_DESCRIPTION("Minimal kprobe enable/disable example");
