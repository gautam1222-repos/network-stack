#ifndef _ETH_H
#define _ETH_H

struct ethernetHeader{
    char preamble[7];
    char sfd[1];
    unsigned char destMAC[6];
    unsigned char srcMAC[6];
    unsigned short int ethernetType;
    char payload[1500];// 20 bytes - IP header, 8 bytes - UDP Header, 1472 bytes - data
    unsigned int crc;
}__attribute__((packed));


static unsigned char MacPool[5][6] = {
    {0xA1,0x34,0x60,0x15,0x03,0x00},
    {0xA2,0x35,0x61,0x16,0x04,0x00},
    {0xA3,0x36,0x62,0x17,0x05,0x00},
    {0xA4,0x37,0x63,0x18,0x06,0x00},
    {0xA5,0x26,0x12,0x12,0x45,0x00},
};

#endif