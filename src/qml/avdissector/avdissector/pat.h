#ifndef PAT_H
#define PAT_H

#include "mpegts.h"

class PATMapEntry {
public:
    uint32_t program_number                  : 16;
    uint32_t reserved_3                      : 3;
    uint32_t network_or_program_map_PID      : 13;  // if(program_number == '0') network_PID
};

//class ProgramMapTable
class ProgramAssociationTable
{
public:
    ProgramAssociationTable();
    /**
     * @pTSBuf: ts原始数据
     * @pPat:
     * return > 0 success. 表示解析的数据长度
     *        <= 0失败
     **/
    int Parse(uint8_t *pTSData);

public:
    uint32_t table_id                        : 8; //固定为0x00 ，标志是该表是PAT
    uint32_t section_syntax_indicator        : 1; //段语法标志位，固定为1
    uint32_t zero                            : 1; //0
    uint32_t reserved_1                      : 2; // 保留位
    uint32_t section_length                  : 12;//表示这个字节后面有用的字节数，包括CRC32
    uint32_t transport_stream_id             : 16;//该传输流的ID，区别于一个网络中其它多路复用的流
    uint32_t reserved_2                      : 2; // 保留位
    uint32_t version_number                  : 5; //范围0-31，表示PAT的版本号
    uint32_t current_next_indicator          : 1; //发送的PAT是当前有效还是下一个PAT有效
    uint32_t section_number                  : 8; //分段的号码。PAT可能分为多段传输，第一段为00，以后每个分段加1，最多可能有256个分段
    uint32_t last_section_number             : 8; //最后一个分段的号码
    uint32_t CRC_32                          : 32;
    std::vector<PATMapEntry> program_number_map_entries;

};

#endif // PAT_H
