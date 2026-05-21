#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#define DRIVER_NAME "simple_serial"
#define UART_BASE 0x3F8
#define UART_IRQ 4

struct simple_serial_port {
  struct uart_port port;
};

static struct simple_serial_port simple_port;
static struct platform_device *simple_pdev;

static int simple_serial_startup(struct uart_port *port) {
  pr_info("%s: starting up\n", DRIVER_NAME);
  return 0;
}

static void simple_serial_shutdown(struct uart_port *port) {
  pr_info("%s: shutting down\n", DRIVER_NAME);
}

static unsigned int simple_serial_tx_empty(struct uart_port *port) {
  return UART_LSR_THRE;
}

static void simple_serial_set_mctrl(struct uart_port *port,
                                    unsigned int mctrl) {}

static unsigned int simple_serial_get_mctrl(struct uart_port *port) {
  return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

static void simple_serial_start_tx(struct uart_port *port) {
  pr_info("%s: starting TX\n", DRIVER_NAME);
}

static void simple_serial_stop_tx(struct uart_port *port) {
  pr_info("%s: stopping TX\n", DRIVER_NAME);
}

static void simple_serial_stop_rx(struct uart_port *port) {
  pr_info("%s: stopping RX\n", DRIVER_NAME);
}

static void simple_serial_enable_ms(struct uart_port *port) {}

static int simple_serial_request_port(struct uart_port *port) { return 0; }

static void simple_serial_release_port(struct uart_port *port) {}

static void simple_serial_config_port(struct uart_port *port, int flags) {
  port->type = PORT_16550A;
}

static int simple_serial_verify_port(struct uart_port *port,
                                     struct serial_struct *ser) {
  return 0;
}

static int simple_serial_handle_irq(struct uart_port *port) {
  pr_info("%s: handling IRQ\n", DRIVER_NAME);
  return 0;
}

static const struct uart_ops simple_serial_ops = {
    .startup = simple_serial_startup,
    .shutdown = simple_serial_shutdown,
    .tx_empty = simple_serial_tx_empty,
    .set_mctrl = simple_serial_set_mctrl,
    .get_mctrl = simple_serial_get_mctrl,
    .stop_tx = simple_serial_stop_tx,
    .start_tx = simple_serial_start_tx,
    .stop_rx = simple_serial_stop_rx,
    .enable_ms = simple_serial_enable_ms,
    .request_port = simple_serial_request_port,
    .release_port = simple_serial_release_port,
    .config_port = simple_serial_config_port,
    .verify_port = simple_serial_verify_port,
};

static struct uart_driver simple_uart_driver = {
    .owner = THIS_MODULE,
    .driver_name = DRIVER_NAME,
    .dev_name = DRIVER_NAME,
    .major = 0,
    .minor = 0,
    .nr = 1,
};

static int __init simple_serial_init(void) {
  int ret;

  simple_pdev = platform_device_register_simple("simple-serial-parent",
                                                PLATFORM_DEVID_AUTO, NULL, 0);
  if (IS_ERR(simple_pdev))
    return PTR_ERR(simple_pdev);

  simple_port.port.dev = &simple_pdev->dev;
  simple_port.port.iotype = UPIO_PORT;
  simple_port.port.iobase = UART_BASE;
  simple_port.port.irq = UART_IRQ;
  simple_port.port.uartclk = 1843200;
  simple_port.port.fifosize = 16;
  simple_port.port.ops = &simple_serial_ops;
  simple_port.port.flags = UPF_BOOT_AUTOCONF;
  simple_port.port.line = 0;
  simple_port.port.handle_irq = simple_serial_handle_irq;

  ret = uart_register_driver(&simple_uart_driver);
  if (ret) {
    platform_device_unregister(simple_pdev);
    simple_pdev = NULL;
    return ret;
  }

  ret = uart_add_one_port(&simple_uart_driver, &simple_port.port);
  if (ret) {
    uart_unregister_driver(&simple_uart_driver);
    platform_device_unregister(simple_pdev);
    simple_pdev = NULL;
    return ret;
  }

  pr_info("%s: driver initialized\n", DRIVER_NAME);
  return 0;
}

static void __exit simple_serial_exit(void) {
  uart_remove_one_port(&simple_uart_driver, &simple_port.port);
  uart_unregister_driver(&simple_uart_driver);

  if (simple_pdev) {
    platform_device_unregister(simple_pdev);
    simple_pdev = NULL;
  }

  pr_info("%s: driver exited\n", DRIVER_NAME);
}

module_init(simple_serial_init);
module_exit(simple_serial_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxy");
