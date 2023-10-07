
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>

#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
#define MAX_WINDOW_SIZE 10
typedef unsigned char uchar_t;

//System configuration information
struct SysConfig_t
{
    float drop_prob;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};
typedef struct SysConfig_t  SysConfig;

//Command line input information
struct Cmd_t
{
    uint16_t src_id;
    uint16_t dst_id;
    char * message;
};
typedef struct Cmd_t Cmd;

//Linked list information
enum LLtype 
{
    llt_string,
    llt_frame,
    llt_integer,
    llt_head
} LLtype;

struct LLnode_t
{
    struct LLnode_t * prev;
    struct LLnode_t * next;
    enum LLtype type;

    void * value;
};
typedef struct LLnode_t LLnode;


//Receiver and sender data structures
struct Receiver_t
{
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_framelist_head
    // 4) recv_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode * input_framelist_head;  // 以Char指针存放
    
    int recv_id;

    uint8_t base;  // 窗口的基序号，即最大已确认帧+1
    uint8_t next_seq_num;
    uint8_t window_size;    
};

struct Sender_t
{
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_cmdlist_head
    // 4) input_framelist_head
    // 5) send_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;    
    LLnode * input_cmdlist_head;    // 以Cmd指针存放
    LLnode * input_framelist_head;  // 以Frame指针存放
    int send_id; // 对应于port

    uint8_t base;  // 窗口的基序号，即最大已确认帧+1
    uint8_t next_seq_num;
    uint8_t window_size;
    struct timeval current_timeval;
};

enum SendFrame_DstType 
{
    ReceiverDst,
    SenderDst
} SendFrame_DstType ;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;


#define MAX_FRAME_SIZE 64

#define SLIDE_WINDOW_SIZE 4

//TODO: You should change this!
//Remember, your frame can be AT MOST 32 bytes!
#define FRAME_PAYLOAD_SIZE 48

// 64 - 48 = 16 - 8 = 8
// +--------+--------+--------+---------+-------+--------+-------+-------+---------+
// |  src   |   dst  |Seq num | ack num | state | windows| ext   |  crc  |  Check  |
// |        |        |        |         |       |  size  | data  |       |  data   |
// +--------+--------+--------+---------+-------+--------+-------+-------+---------+
// |  1byte | 1byte  | 1byte  | 1byte   | 1byte | 1byte  | 8byte |2bytes |  48byte |
// +--------+--------+--------+---------+-------+--------+-------+-------+---------+

struct Frame_t
{
    uint8_t src_port;      // 源端口
    uint8_t dst_port;      // 目的端口
    uint8_t seq_num;        // 帧的序号
    uint8_t ack_num;        // 确认号
    uint8_t state;     // 帧的状态指示
    uint8_t window_size;    // 窗口大小

    uint64_t ext;       // 扩展数据 

    uint16_t crc;       // 校验和
    char data[FRAME_PAYLOAD_SIZE];  // 数据
};
typedef struct Frame_t Frame;

//Declare global variables here
//DO NOT CHANGE: 
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender * glb_senders_array;
Receiver * glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;
int CORRUPTION_BITS;
#endif 
