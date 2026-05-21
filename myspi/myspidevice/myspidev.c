#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#define FAKE_SPI_BUS_NUM   42
#define FAKE_SPI_CS_NUM    0
#define FAKE_SPI_MAX_CS    1
#define FAKE_SPI_SPEED_HZ  1000000

static struct spi_controller *fake_ctlr;
static struct spi_device *fake_spi_dev;
static struct platform_device *fake_pdev;

static int fake_spi_transfer_one(struct spi_controller *ctlr,
                 struct spi_device *spi,
                 struct spi_transfer *xfer)
{
    size_t i;
    const u8 *tx = xfer->tx_buf;
    u8 *rx = xfer->rx_buf;

    pr_info("fake_spi: transfer_one len=%u\n", xfer->len);

    if (rx && tx) {
        for (i = 0; i < xfer->len; i++)
            rx[i] = tx[i];
    } else if (rx) {
        memset(rx, 0x5A, xfer->len);
    }

    spi_finalize_current_transfer(ctlr);
    return 0;
}

static int fake_spi_create_device(void)
{
    struct spi_board_info board_info = {
        .modalias = "zxyspidev",
        .max_speed_hz = FAKE_SPI_SPEED_HZ,
        .bus_num = -1,
        .chip_select = FAKE_SPI_CS_NUM,
        .mode = SPI_MODE_0,
    };

    fake_spi_dev = spi_new_device(fake_ctlr, &board_info);
    if (!fake_spi_dev) {
        pr_err("fake_spi: spi_new_device failed\n");
        return -ENODEV;
    }

    fake_spi_dev->bits_per_word = 8;

    pr_info("fake_spi: created device %s cs=%u\n",
        dev_name(&fake_spi_dev->dev),
        spi_get_chipselect(fake_spi_dev, 0));

    return 0;
}

static int __init fake_spi_init(void)
{
    int ret;

    fake_pdev = platform_device_register_simple("fake-spi-parent", -1, NULL, 0);
    if (IS_ERR(fake_pdev)) {
        ret = PTR_ERR(fake_pdev);
        pr_err("fake_spi: platform_device_register_simple failed: %d\n", ret);
        return ret;
    }

    fake_ctlr = spi_alloc_host(&fake_pdev->dev, 0);
    if (!fake_ctlr) {
        pr_err("fake_spi: spi_alloc_host failed\n");
        platform_device_unregister(fake_pdev);
        fake_pdev = NULL;
        return -ENOMEM;
    }

    fake_ctlr->bus_num = -1;
    fake_ctlr->num_chipselect = FAKE_SPI_MAX_CS;
    fake_ctlr->mode_bits = SPI_MODE_0;
    fake_ctlr->bits_per_word_mask = SPI_BPW_MASK(8);
    fake_ctlr->transfer_one = fake_spi_transfer_one;

    ret = spi_register_controller(fake_ctlr);
    if (ret) {
        pr_err("fake_spi: spi_register_controller failed: %d\n", ret);
        spi_controller_put(fake_ctlr);
        fake_ctlr = NULL;
        platform_device_unregister(fake_pdev);
        fake_pdev = NULL;
        return ret;
    }

    pr_info("fake_spi: controller registered as spi%u\n", fake_ctlr->bus_num);

    ret = fake_spi_create_device();
    if (ret) {
        spi_unregister_controller(fake_ctlr);
        fake_ctlr = NULL;
        platform_device_unregister(fake_pdev);
        fake_pdev = NULL;
        return ret;
    }

    pr_info("fake_spi: init complete\n");
    return 0;
}

static void __exit fake_spi_exit(void)
{
    if (fake_spi_dev) {
        spi_unregister_device(fake_spi_dev);
        fake_spi_dev = NULL;
    }

    if (fake_ctlr) {
        spi_unregister_controller(fake_ctlr);
        fake_ctlr = NULL;
    }

    if (fake_pdev) {
        platform_device_unregister(fake_pdev);
        fake_pdev = NULL;
    }

    pr_info("fake_spi: exited\n");
}

module_init(fake_spi_init);
module_exit(fake_spi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxy");
MODULE_DESCRIPTION("Fake SPI controller for testing");
