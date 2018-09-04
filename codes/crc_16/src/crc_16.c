/*
 ============================================================================
 Name        : crc_16.c
 Author      : Renan Augusto Starke
 Version     :
 Copyright   : Instituto Federal de Santa Catarina
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint16_t CRC16_2(uint8_t *buf, int len)
{
  uint crc = 0xFFFF;
  int i;

  for (i = 0; i < len; i++)
  {
    crc ^= (uint16_t)buf[i];          // XOR byte into least sig. byte of crc

    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;
}


int main(void) {

	uint8_t pkg_0[] = {0x07, 0x1e, 0x83, 0x88, 0xff};
	uint8_t pkg_1[] = {0x08, 0x1e, 0x83, 0x88, 0xff};
	uint8_t pkg_2[] = {0x08, 0x1e, 0x08, 0x00, 0xda, 0x7f, 0xff, 0x01, 0x18, 0x01, 0x00};
	uint8_t pkg_3[] = {0x07, 0x1e, 0x83, 0x88, 0xff};

	printf("CRC pkg[%d]: %x\n", 0, CRC16_2(pkg_0,2));
	printf("CRC pkg[%d]: %x\n", 1, CRC16_2(pkg_1,2));
	printf("CRC pkg[%d]: %x\n", 2, CRC16_2(pkg_2,sizeof(pkg_2)));

	return EXIT_SUCCESS;
}
