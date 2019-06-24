#include "pat.h"

ProgramAssociationTable::ProgramAssociationTable()
{

}

// Parse PAT
int ProgramAssociationTable::Parse(uint8_t *pTSData)
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
