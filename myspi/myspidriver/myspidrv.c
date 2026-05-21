#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/spi/spi.h>

#define SPI_BUS 0
#define SPI_BUS_CS1 0
#define SPI_BUS_SPEED 1000000

static int spi_device_probe(struct spi_device *spi) {
  int ret;
  u8 tx[] = {0xAA};
  u8 rx[ARRAY_SIZE(tx)];
  spi->max_speed_hz = SPI_BUS_SPEED;
  spi->mode = SPI_MODE_0;
  spi->bits_per_word = 8;
  ret = spi_setup(spi);
  if (ret) {
    pr_err("spi_setup failed\n");
    return ret;
  }
  ret = spi_write_then_read(spi, tx, ARRAY_SIZE(tx), rx, ARRAY_SIZE(rx));
  if (ret) {
    pr_err("spi_write_then_read failed\n");
    return ret;
  }
  pr_info("SPI device probed successfully\n");
  return 0;
}

static void spi_device_remove(struct spi_device *spi) {
  pr_info("SPI device removed\n");
}

static const struct spi_device_id id_table[] = {
    {.name = "zxyspidev"},
    {},
};

static const struct of_device_id spi_device_dt_ids[] = {
    {.compatible = "myspidev"}, {}};

static struct spi_driver spi_device_driver = {
    .driver =
        {
            .name = "zxy_spidev",
            .of_match_table = spi_device_dt_ids,
        },
    .probe = spi_device_probe,
    .remove = spi_device_remove,
    .id_table = id_table,
};

module_spi_driver(spi_device_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxy");
MODULE_DESCRIPTION("Simple SPI test driver");
