#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>

#define DRIVER_NAME "simple_pci"
#define PCI_VENDOR_ID_EXAMPLE 0x8086
#define PCI_DEVICE_ID_EXAMPLE 0x100e

static struct pci_device_id pci_ids[] = {
    {
        PCI_DEVICE(PCI_VENDOR_ID_EXAMPLE, PCI_DEVICE_ID_EXAMPLE),
    },
    {
        0,
    },
};

MODULE_DEVICE_TABLE(pci, pci_ids);

struct simple_pci_dev {
  struct pci_dev *pdev;
  void __iomem *mmio_base;
};

static int simple_pci_probe(struct pci_dev *pdev,
                            const struct pci_device_id *ent) {
  struct simple_pci_dev *dev;
  int err;
  dev = kzalloc(sizeof(*dev), GFP_KERNEL);
  if (!dev)
    return -ENOMEM;
  dev->pdev = pdev;
  pci_set_drvdata(pdev, dev);
  err = pci_enable_device(pdev);
  if (err)
    goto err_free_dev;
  
  pr_info(DRIVER_NAME ": device probed successfully\n");
  return 0;
err_free_dev:
  pr_err(DRIVER_NAME ": pci_enable_device failed: %d\n", err);
  kfree(dev);
  return err;
}

static void simple_pci_remove(struct pci_dev *pdev) {
  pr_info(DRIVER_NAME ": remove called for %s\n", pci_name(pdev));
  struct simple_pci_dev *dev = pci_get_drvdata(pdev);
  pci_disable_device(pdev);
  kfree(dev);
  pr_info(DRIVER_NAME ": device removed\n");
}

static struct pci_driver simple_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = pci_ids,
    .probe = simple_pci_probe,
    .remove = simple_pci_remove,
};

static int __init simple_pci_init(void) {
  pr_info("zxy pci driver init");
  return pci_register_driver(&simple_pci_driver);
}

static void __exit simple_pci_exit(void) {
	pr_info("zxy pci driver exit");
  pci_unregister_driver(&simple_pci_driver);
}

module_init(simple_pci_init);
module_exit(simple_pci_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxy");
