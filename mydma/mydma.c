#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/slab.h>
#define DRIVER_NAME "dma_nic_driver"
#define TX_BUFFER_SIZE 4096
struct dma_nic_device {
  struct pci_dev *pdev;
  struct net_device *netdev;
  void *tx_buffer;
  dma_addr_t tx_buffer_dma_addr;
};

static int dma_nic_probe(struct pci_dev *pdev,
                         const struct pci_device_id *ent) {
  struct dma_nic_device *dev;
  int err = 0;
  dev = kzalloc(sizeof(struct dma_nic_device), GFP_KERNEL);
  if (!dev)
    return -ENOMEM;
  dev->pdev = pdev;

  // Enable the device
  err = pci_enable_device(pdev);
  if (err) {
    dev_err(&pdev->dev, "Failed to enable PCI device\n");
    goto err_free_dev;
  }

  pci_set_master(pdev);
  err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
  if (err) {
    err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
    if (err) {
      dev_err(&pdev->dev, "No suitable DMA configuration\n");
      goto err_disable_device;
    }
  }

  // Allocate memory buffer for DMA transfer
  dev->tx_buffer = dma_alloc_coherent(&pdev->dev, TX_BUFFER_SIZE,
                                      &dev->tx_buffer_dma_addr, GFP_KERNEL);
  if (!dev->tx_buffer) {
    dev_err(&pdev->dev, "Failed to allocate DMA buffer\n");
    err = -ENOMEM;
    goto err_disable_device;
  }

  // Initialize network device
  dev->netdev = alloc_etherdev(0);
  if (!dev->netdev) {
    err = -ENOMEM;
    goto err_free_dma_buffer;
  }

  // Set up DMA descriptors, initialize NIC, etc. (not shown for brevity)
  pci_set_drvdata(pdev, dev);
  dev_info(&pdev->dev, "probe called\n");
  dev_info(&pdev->dev, "DMA buffer cpu=%p dma=%pad\n", dev->tx_buffer,
           &dev->tx_buffer_dma_addr);
  return 0;

err_free_dma_buffer:
  dma_free_coherent(&pdev->dev, TX_BUFFER_SIZE, dev->tx_buffer,
                    dev->tx_buffer_dma_addr);
err_disable_device:
  pci_disable_device(pdev);
err_free_dev:
  kfree(dev);
  return err;
}

static void dma_nic_remove(struct pci_dev *pdev) {
  struct dma_nic_device *dev = pci_get_drvdata(pdev);
  // Clean up resources
  if (dev) {
    if (dev->netdev)
      free_netdev(dev->netdev);
    if (dev->tx_buffer)
      dma_free_coherent(&pdev->dev, TX_BUFFER_SIZE, dev->tx_buffer,
                        dev->tx_buffer_dma_addr);
    pci_disable_device(pdev);
    pci_set_drvdata(pdev, NULL);
    kfree(dev);
  }
  dev_info(&pdev->dev, "remove called\n");
}

static struct pci_device_id dma_nic_id_table[] = {
    {PCI_DEVICE(0x8086, 0x100e)}, // Example PCI vendor and device IDs
    {0},
};
MODULE_DEVICE_TABLE(pci, dma_nic_id_table);
static struct pci_driver dma_nic_driver = {
    .name = DRIVER_NAME,
    .id_table = dma_nic_id_table,
    .probe = dma_nic_probe,
    .remove = dma_nic_remove,
};

static int __init dma_nic_init(void) {
  return pci_register_driver(&dma_nic_driver);
}
static void __exit dma_nic_exit(void) {
  pci_unregister_driver(&dma_nic_driver);
}
module_init(dma_nic_init);
module_exit(dma_nic_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("BPB");
MODULE_DESCRIPTION("Example DMA NIC Driver");
