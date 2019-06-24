#include "pmt.h"

ProgramMapTable::ProgramMapTable()
{

}

int ProgramMapTable::Parse(uint8_t * pTSData)
{
    int pos = 12, len = 0;
    int i = 0;
    this->table_id                            = pTSData[0];
    this->section_syntax_indicator            = pTSData[1] >> 7;
    this->zero                                = pTSData[1] >> 6;
    this->reserved_1                            = pTSData[1] >> 4;
    this->section_length                        = (pTSData[1] & 0x0F) << 8 | pTSData[2];
    this->program_number                        = pTSData[3] << 8 | pTSData[4];
    this->reserved_2                            = pTSData[5] >> 6;
    this->version_number                        = pTSData[5] >> 1 & 0x1F;
    this->current_next_indicator                = (pTSData[5] << 7) >> 7;
    this->section_number                        = pTSData[6];
    this->last_section_number                    = pTSData[7];
    this->reserved_3                            = pTSData[8] >> 5;
    this->PCR_PID                                = ((pTSData[8] << 8) | pTSData[9]) & 0x1FFF;
    this->reserved_4                            = pTSData[10] >> 4;
    this->program_info_length                    = (pTSData[10] & 0x0F) << 8 | pTSData[11];
    // Get CRC_32
    len = this->section_length + 3;
    this->CRC_32                = (pTSData[len-4] & 0x000000FF) << 24
                                  | (pTSData[len-3] & 0x000000FF) << 16
                                  | (pTSData[len-2] & 0x000000FF) << 8
                                  | (pTSData[len-1] & 0x000000FF);
    // program info descriptor
    if ( this->program_info_length != 0 )
        pos += this->program_info_length;
    // Get stream type and PID
    for ( ; pos <= (this->section_length + 2 ) -  4; )
    {
        this->stream_type                            = pTSData[pos];
        this->reserved_5                            = pTSData[pos+1] >> 5;
        this->elementary_PID                        = ((pTSData[pos+1] << 8) | pTSData[pos+2]) & 0x1FFF;
        this->reserved_6                            = pTSData[pos+3] >> 4;
        this->ES_info_length                        = (pTSData[pos+3] & 0x0F) << 8 | pTSData[pos+4];
        // Store in es
        es[i].type = this->stream_type;
        es[i].pid = this->elementary_PID;
        if ( this->ES_info_length != 0 )
        {
            pos = pos+5;
            pos += this->ES_info_length;
        }
        else
        {
            pos += 5;
        }
        i++;
    }
}
