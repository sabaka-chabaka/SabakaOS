#pragma once
#include <stdint.h>

#define RTL_REG_MAC0        0x00
#define RTL_REG_MAR0        0x08
#define RTL_REG_TXSTATUS0   0x10
#define RTL_REG_TXADDR0     0x20
#define RTL_REG_RXBUF       0x30
#define RTL_REG_CHIPCMD     0x37
#define RTL_REG_RXBUFTAIL   0x38
#define RTL_REG_RXBUFHEAD   0x3A
#define RTL_REG_INTRMASK    0x3C
#define RTL_REG_INTRSTATUS  0x3E
#define RTL_REG_TXCONFIG    0x40
#define RTL_REG_RXCONFIG    0x44
#define RTL_REG_MPC         0x4C
#define RTL_REG_CFG9346     0x52
#define RTL_REG_CONFIG1     0x52

#define RTL_CMD_RESET       0x10
#define RTL_CMD_RX_ENABLE   0x08
#define RTL_CMD_TX_ENABLE   0x04

#define RTL_INT_ROK         0x0001
#define RTL_INT_RER         0x0002
#define RTL_INT_TOK         0x0004
#define RTL_INT_TER         0x0008
#define RTL_INT_RXOVW       0x0010
#define RTL_INT_FOVW        0x0040

#define RTL_RX_ACCEPT_ALL   (1<<0)
#define RTL_RX_ACCEPT_PHYS  (1<<1)
#define RTL_RX_ACCEPT_MULTI (1<<2)
#define RTL_RX_ACCEPT_BCAST (1<<3)
#define RTL_RX_WRAP         (1<<7)
#define RTL_RX_BUFSZ_32K   (1<<11)

#define RTL_TX_OWN          (1<<13)
#define RTL_TX_OK           (1<<15)

#define RTL_RX_BUF_SIZE     (32*1024 + 16 + 1500)
#define RTL_TX_BUF_SIZE     1536
#define RTL_TX_NUM          4

typedef void (*rtl_rx_callback)(const uint8_t* data, uint16_t len);

bool     rtl8139_init();
bool     rtl8139_present();
void     rtl8139_set_rx_callback(rtl_rx_callback cb);
bool     rtl8139_send(const uint8_t* data, uint16_t len);
void     rtl8139_irq_handler();
void     rtl8139_get_mac(uint8_t mac[6]);