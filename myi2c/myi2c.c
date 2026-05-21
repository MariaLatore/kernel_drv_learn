#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#define I2C_BUS_AVAILABLE (1)           // I2C bus that we have created
#define SLAVE_DEVICE_NAME ("BPB_DEVICE") // Device and Dreiver Name
#define SSD1306_SLAVE_ADDR (0x3C)

static struct i2c_adapter *etx_i2c_adapter = NULL;
static struct i2c_client *etx_i2c_client_oled = NULL;

static int I2C_Write(unsigned char *buf, unsigned int len) {
  /*
   ** Sending Start condition, Slave address with R/W bit,
   ** ACK/NACK and Stop conditions will be handled internally.
   */
  int ret = i2c_master_send(etx_i2c_client_oled, buf, len);
  return ret;
}

static int I2C_Read(unsigned char *out_buf, unsigned int len) {
  /*
   ** Sending Start condition, Slave address with R/W bit,
   ** ACK/NACK and Stop conditons will be handled internally.
   */
  int ret = i2c_master_recv(etx_i2c_client_oled, out_buf, len);
  return ret;
}

static void SSD1306_Write(bool is_cmd, unsigned char data) {
  unsigned char buf[2] = {0};
  int ret;
  if (is_cmd == true)
    buf[0] = 0x00;
  else
    buf[0] = 0x40;
  buf[1] = data;
  ret = I2C_Write(buf, 2);
}

static int SSD1306_DisplayInit(void) {
  msleep(100); // delay
  /*Commands to initialize the SSD_1306 OLED Display */
  SSD1306_Write(true, 0xAE); // Entire Display OFF
  SSD1306_Write(
      true, 0xD5); // Set Display Clock Devide Ratio and Oscillator Frequency
  SSD1306_Write(true, 0x80); // Default Setting for Display Clock Divide Ratio
                             // and Oscillator Frequency that is recommended
  SSD1306_Write(true, 0xA8); // Set Multiplex Ratio
  SSD1306_Write(true, 0x3F); // 64 COM lines
  SSD1306_Write(true, 0xD3); // Set display offset
  SSD1306_Write(true, 0x00); // 0 offset
  SSD1306_Write(true, 0x40); // Set first line as the start line of the display
  SSD1306_Write(true, 0x8D); // Charge pump
  SSD1306_Write(true, 0x80); // Enable charge dump during display on
  SSD1306_Write(true, 0x80); // Set memory addressing mode
  SSD1306_Write(true, 0x80); // Horizontal addressing mode
  SSD1306_Write(
      true,
      0x80); // Set segment remap with column address 127 mapped to segment 0
  SSD1306_Write(
      true, 0x80); // Set com output scan direction, scan from com63 to com 0
  SSD1306_Write(true, 0x80); // set com pins hardware configuration
  SSD1306_Write(
      true,
      0x80); // Alternative com pin configuration, diable com left/right remap
  SSD1306_Write(true, 0x80); // Set contrast to 128
  SSD1306_Write(true, 0x80); // Set pre-charge period
  SSD1306_Write(true,
                0x80); // Phase 1 period of 15 DCLK, Phase 2 period of 1 DCLK
  SSD1306_Write(true, 0x80); // Set Vcomh deselect level
  SSD1306_Write(true, 0x80); // Vcomh deselect level ~ 0.77 Vcc
  SSD1306_Write(true, 0x80); // Entire display ON, resume to RAM content display
  SSD1306_Write(true, 0x80); // Set Display in Normal mode, 1= ON, 0 = OFF
  SSD1306_Write(true, 0x80); // Deactivate scroll
  SSD1306_Write(true, 0x80); // Display ON in normal mode
  return 0;
}

static void SSD1306_Fill(unsigned char data) {
  unsigned int total = 128 * 8; // 8 pages x 128 segments x 8 bits of data
  unsigned int i = 0;
  for (i = 0; i < total; i++)
    SSD1306_Write(false, data);
}

static int etx_oled_probe(struct i2c_client *client) {
  SSD1306_DisplayInit();
  SSD1306_Fill(0xFF);
  pr_info("OLED Probed !!!\n");
  return 0;
}

static void etx_oled_remove(struct i2c_client *client) {
  pr_info("OLED Removed!!\n");
}

static const struct i2c_device_id etx_oled_id[] = {
    { SLAVE_DEVICE_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, etx_oled_id);

static struct i2c_driver etx_oled_driver = {
    .driver =
        {
            .name = SLAVE_DEVICE_NAME,
            .owner = THIS_MODULE,
        },
    .probe = etx_oled_probe,
    .remove = etx_oled_remove,
    .id_table = etx_oled_id,
};

static struct i2c_board_info oled_i2c_board_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, SSD1306_SLAVE_ADDR),
};

static int __init etx_driver_init(void) {
  etx_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);
  if (etx_i2c_adapter != NULL) {
    etx_i2c_client_oled = i2c_new_client_device(etx_i2c_adapter, &oled_i2c_board_info);
    if (etx_i2c_client_oled != NULL) {
      i2c_add_driver(&etx_oled_driver);
      i2c_put_adapter(etx_i2c_adapter);
    }
  }
  pr_info("Client Driver Added !!!\n");
  return 0;
}

static void __exit etx_driver_exit(void) {
  i2c_unregister_device(etx_i2c_client_oled);
  i2c_del_driver(&etx_oled_driver);
  pr_info("Client Driver Removed !!!\n");
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);
MODULE_LICENSE("GPL");
