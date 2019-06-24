#ifndef MPEGTS_H
#define MPEGTS_H

#include <vector>
#include <memory>
#include <QDebug>

class Parser {
public:
    virtual int Parse(unsigned char *pTSData) = 0;
};

class TsHeader : public Parser
{
public:
         unsigned sync_byte                    :8;      //同步字节，固定为0x47 ，表示后面的是一个TS分组，当然，后面包中的数据是不会出现0x47的
         unsigned transport_error_indicator       :1;      //传输错误标志位，一般传输错误的话就不会处理这个包了
         unsigned payload_unit_start_indicator    :1;      //有效负载的开始标志，根据后面有效负载的内容不同功能也不同， 0x01表示含有PSI或者PES头
         // payload_unit_start_indicator为1时，在前4个字节之后会有一个调整字节，它的数值决定了负载内容的具体开始位置。
         unsigned transport_priority              :1;      //传输优先级位，1表示高优先级
         unsigned PID                          :13;     //有效负载数据的类型，0x0表示后面负载内容为PAT，不同的PID表示不同的负载
         unsigned transport_scrambling_control     :2;      //加密标志位,00表示未加密
         unsigned adaptation_field_control          :2;      //调整字段控制,。01仅含有效负载，10仅含调整字段，11含有调整字段和有效负载。为00的话解码器不进行处理。
         unsigned continuity_counter              :4;      //一个4bit的计数器，范围0-15，据计数器读数，接收端可判断是否有包丢失及包传送顺序错误。显然，包头对TS包具有同步、识别、检错及加密功能
public:
         /**
          * @pTSBuf: ts原始数据
          * @pheader: ts header
          * @return > 0: 解析的header的长度
          **/
         int Parse(unsigned char *pTSData);
};



class TsAdaptationField : public Parser
{
public:
    uint64_t adaptation_field_length :8;
    //if(adaptation_field_length >0)
    uint64_t discontinuity_indicator :1;
    uint64_t random_access_indicator:1;
    uint64_t elementary_stream_priority_indicator:1;
    uint64_t PCR_flag:1;
    uint64_t OPCR_flag:1;
    uint64_t splicing_point_flag:1;
    uint64_t transport_private_data_flag:1;
    uint64_t adaptation_field_extension_flag:1;

    //if(PCR_flag == '1')
    uint64_t program_clock_reference_base:33;
    uint64_t reserved_1:6;
    uint64_t program_clock_reference_extension:9;


    //if(OPCR_flag == '1')
    uint64_t original_program_clock_reference_base:33;
    uint64_t reserved_2:6;
    uint64_t original_program_clock_reference_extension:9;
    //if (splicing_point_flag == '1')
    uint64_t splice_countdown:8;
    //if(transport_private_data_flag == '1')
    uint64_t transport_private_data_length:8; //立即跟随transport_private_data_length长度的data
    std::vector<char> private_data_byte;

    //if (adaptation_field_extension_flag == '1' )
    unsigned adaptation_field_extension_length:8;
    unsigned ltw_flag:1;
    unsigned piecewise_rate_flag:1;
    unsigned seamless_splice_flag:1;
    unsigned reserved_3:5;
    //if (ltw_flag == '1')
    unsigned ltw_valid_flag:1;
    unsigned ltw_offset:15;
    //if (piecewise_rate_flag == '1')
    unsigned reserved_4:2;
    unsigned piecewise_rate:22;
    //if (seamless_splice_flag == '1')
    unsigned splice_type:4;
    unsigned DTS_next_AU_1:3;
    unsigned marker_bit_1:1;
    unsigned DTS_next_AU_2:15;
    unsigned marker_bit_2:1;
    unsigned DTS_next_AU_3:15;
    unsigned marker_bit_3:1;
    unsigned reserved:8;
    unsigned stuffing_byte:8;
};






class TsPacket : public Parser
{
public:
    TsHeader header;
    std::shared_ptr<TsAdaptationField> adaptationField;
    std::shared_ptr<TsPMT> pmt;
    std::shared_ptr<TsPAT> pat;
public:
    int Parse(unsigned char *pTSData);
};


class mpegts
{
public:
    mpegts();
    std::vector<TsPacket> tsPackets;
};

#endif // MPEGTS_H
