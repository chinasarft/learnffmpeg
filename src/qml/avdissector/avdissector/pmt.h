#ifndef PMT_H
#define PMT_H

#include "mpegts.h"


class PMTMapEntry {
public:
    uint32_t stream_type                     : 8;
    uint32_t reserved_5                      : 3;
    uint32_t elementary_PID                  : 13;
    uint32_t reserved_6                      : 4;
    uint32_t ES_info_length                  : 12;
};

class ProgramMapTable
{
public:
    ProgramMapTable();

public:
     uint32_t table_id                        : 8;
     uint32_t section_syntax_indicator        : 1;
     uint32_t zero                            : 1;
     uint32_t reserved_1                      : 2;
     uint32_t section_length                  : 12;
     uint32_t program_number                  : 16;
     uint32_t reserved_2                      : 2;
     uint32_t version_number                  : 5;
     uint32_t current_next_indicator          : 1;
     uint32_t section_number                  : 8;
     uint32_t last_section_number             : 8;
     uint32_t reserved_3                      : 3;
     uint32_t PCR_PID                         : 13;
     uint32_t reserved_4                      : 4;
     uint32_t program_info_length             : 12;
     uint32_t CRC_32                          : 32;
     std::vector<PMTMapEntry> stream_id_map_entries;
public:
     int Parse(uint8_t *pTSData);
} ;

#endif // PMT_H
