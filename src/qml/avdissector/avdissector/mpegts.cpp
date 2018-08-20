#include "mpegts.h"

int TsHeader::Parse(unsigned char *pTSData)
{
    this->sync_byte = pTSData[0];
    if (this->sync_byte != 0x47)
        return -1;
#if 0
    this->transport_error_indicator       = pTSData[1] >> 7;
    this->payload_unit_start_indicator    = pTSData[1] >> 6 & 0x01;
    this->transport_priority             = pTSData[1] >> 5 & 0x01;
    this->PID                         = (pTSData[1] & 0x1F) << 8 | pTSData[2];
    this->transport_scrambling_control   = pTSData[3] >> 6;
    this->adaptation_field_control         = pTSData[3] >> 4 & 0x03;
    this->continuity_counter            = pTSData[3] & 0x0F;
#endif

    memcpy(this, pTSData, 4);
    return 4;
}

// Parse PAT
int TsPAT::Parse(unsigned char *pTSData)
{
    TsHeader TSheader;
    if (TSheader.Parse(pTSData) != 0)
        return -1;
    if (TSheader.payload_unit_start_indicator == 0x01) // 表示含有PSI或者PES头
    {
        if (TSheader.PID == 0x0)  // 表示PAT
        {
             int iBeginlen = 4;
             int adaptation_field_length = pTSData[4];
             switch(TSheader.adaptation_field_control)
             {
             case 0x0:                                   // reserved for future use by ISO/IEC
                  return -1;
             case 0x1:                                   // 无调整字段，仅含有效负载
                  iBeginlen += pTSData[iBeginlen] + 1;  // + pointer_field
                  break;
             case 0x2:                                    // 仅含调整字段，无有效负载
                  return -1;
             case 0x3:// 调整字段后含有效负载
                 if (adaptation_field_length > 0)
                 {
                      iBeginlen += 1;                   // adaptation_field_length占8位
                      iBeginlen += adaptation_field_length; // + adaptation_field_length
                 }
                 else
                 {
                      iBeginlen += 1;                       // adaptation_field_length占8位
                 }
                 iBeginlen += pTSData[iBeginlen] + 1;           // + pointer_field
                 break;
            default:
                 break;
            }
            unsigned char *pPATData = pTSData + iBeginlen;
            this->table_id                    = pPATData[0];
            this->section_syntax_indicator    = pPATData[1] >> 7;
            this->zero                        = pPATData[1] >> 6 & 0x1;
            this->reserved_1                  = pPATData[1] >> 4 & 0x3;
            this->section_length              = (pPATData[1] & 0x0F) << 8 |pPATData[2];
            this->transport_stream_id         = pPATData[3] << 8 | pPATData[4];
            this->reserved_2                  = pPATData[5] >> 6;
            this->version_number              = pPATData[5] >> 1 &  0x1F;
            this->current_next_indicator      = (pPATData[5] << 7) >> 7;
            this->section_number              = pPATData[6];
            this->last_section_number         = pPATData[7];
            int len = 0;
            len = 3 + this->section_length;
            this->CRC_32                      = (pPATData[len-4] & 0x000000FF) << 24
                                                | (pPATData[len-3] & 0x000000FF) << 16
                                                | (pPATData[len-2] & 0x000000FF) << 8
                                                | (pPATData[len-1] & 0x000000FF);

            int n = 0;
            for ( n = 0; n < (this->section_length - 12); n += 4 )
            {
                TsPATMapEntry entry;
                 entry.program_number = pPATData[8 + n ] << 8 | pPATData[9 + n ];
                 entry.reserved_3                = pPATData[10 + n ] >> 5;
                 entry.network_or_program_map_PID = (pPATData[10 + n ] & 0x1F) << 8 |pPATData[11 + n ];
                 /*
                 if ( this->program_number != 0x00) {
                     // 有效的PMT的PID,然后通过这个PID值去查找PMT包
                     program_map_PID = (pPATData[10 + n] & 0x1F) << 8 |pPATData[11 + n];
                 }
                 */
                 this->program_number_map_entries.push_back(entry);
            }
            return 0;
         }
    }
    return -1;
}

int TsPMT::Parse(unsigned char *pTSData)
{
    return 0;
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
/*
void ParseTsPMT(unsigned char * pTSData, TS_PMT * pPmt)
{
    int pos = 12, len = 0;
    int i = 0;
    pPmt->table_id                            = pTSData[0];
    pPmt->section_syntax_indicator            = pTSData[1] >> 7;
    pPmt->zero                                = pTSData[1] >> 6;
    pPmt->reserved_1                            = pTSData[1] >> 4;
    pPmt->section_length                        = (pTSData[1] & 0x0F) << 8 | pTSData[2];
    pPmt->program_number                        = pTSData[3] << 8 | pTSData[4];
    pPmt->reserved_2                            = pTSData[5] >> 6;
    pPmt->version_number                        = pTSData[5] >> 1 & 0x1F;
    pPmt->current_next_indicator                = (pTSData[5] << 7) >> 7;
    pPmt->section_number                        = pTSData[6];
    pPmt->last_section_number                    = pTSData[7];
    pPmt->reserved_3                            = pTSData[8] >> 5;
    pPmt->PCR_PID                                = ((pTSData[8] << 8) | pTSData[9]) & 0x1FFF;
    pPmt->reserved_4                            = pTSData[10] >> 4;
    pPmt->program_info_length                    = (pTSData[10] & 0x0F) << 8 | pTSData[11];
    // Get CRC_32
    len = pPmt->section_length + 3;
    pPmt->CRC_32                = (pTSData[len-4] & 0x000000FF) << 24
                                  | (pTSData[len-3] & 0x000000FF) << 16
                                  | (pTSData[len-2] & 0x000000FF) << 8
                                  | (pTSData[len-1] & 0x000000FF);
    // program info descriptor
    if ( pPmt->program_info_length != 0 )
        pos += pPmt->program_info_length;
    // Get stream type and PID
    for ( ; pos <= (pPmt->section_length + 2 ) -  4; )
    {
        pPmt->stream_type                            = pTSData[pos];
        pPmt->reserved_5                            = pTSData[pos+1] >> 5;
        pPmt->elementary_PID                        = ((pTSData[pos+1] << 8) | pTSData[pos+2]) & 0x1FFF;
        pPmt->reserved_6                            = pTSData[pos+3] >> 4;
        pPmt->ES_info_length                        = (pTSData[pos+3] & 0x0F) << 8 | pTSData[pos+4];
        // Store in es
        es[i].type = pPmt->stream_type;
        es[i].pid = pPmt->elementary_PID;
        if ( pPmt->ES_info_length != 0 )
        {
            pos = pos+5;
            pos += pPmt->ES_info_length;
        }
        else
        {
            pos += 5;
        }
        i++;
    }
}
*/

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
