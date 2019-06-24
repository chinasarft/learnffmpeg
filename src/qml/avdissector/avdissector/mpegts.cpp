#include "mpegts.h"

int TsHeader::Parse(unsigned char *pTSData)
{
    this->sync_byte = pTSData[0];
    if (this->sync_byte != 0x47)
        return -1;

    this->transport_error_indicator       = pTSData[1] >> 7;
    this->payload_unit_start_indicator    = pTSData[1] >> 6 & 0x01;
    this->transport_priority             = pTSData[1] >> 5 & 0x01;
    this->PID                         = (pTSData[1] & 0x1F) << 8 | pTSData[2];
    this->transport_scrambling_control   = pTSData[3] >> 6;
    this->adaptation_field_control         = pTSData[3] >> 4 & 0x03;
    this->continuity_counter            = pTSData[3] & 0x0F;

    return 4;
}


int TsPacket::Parse(unsigned char *pTSData)
{
    int nHeaderParsed = header.Parse(pTSData);
    if (nHeaderParsed < 0)
        return nHeaderParsed;
    int nAdaptationParsed = 0;
    if (header.adaptation_field_control == 2 || header.adaptation_field_control == 3) {

    }
    if (header.adaptation_field_control == 1 || header.adaptation_field_control == 3) {

    }
    return 0;
}


#include <stdio.h>
mpegts::mpegts()
{
    FILE * f = fopen("/Users/liuye/Documents/ffmpeg/buildlearn/src/cutav2ts/Debug/a.ts", "rb");
    if (f == NULL){
        qDebug()<<"open file fail";
        return;
    }
    char buf[188] = {0};
    int n = fread(buf, 1, sizeof(buf), f);
    if (n != 188)
        qDebug()<<"fread error";
    TsPacket pkt;
    pkt.Parse((unsigned char *)buf);
}
