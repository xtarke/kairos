# Flash mode, valid values are same as for esptool.py - qio,qout,dio.dout
# Some Wemos D1 does not boot when flash with qio
FLASH_MODE ?= dio

PROGRAM=mqtt_client
#EXTRA_COMPONENTS = extras/dhcpserver extras/wificfg extras/paho_mqtt_c
EXTRA_COMPONENTS = extras/dhcpserver ./wificfg ./paho_mqtt_c
# include ../../common.mk
EXTRA_CFLAGS=-DLWIP_MDNS_RESPONDER=1 -DLWIP_NUM_NETIF_CLIENT_DATA=1 -DLWIP_NETIF_EXT_STATUS_CALLBACK=1
include /mnt/d/dev/esp-open-rtos/common.mk
