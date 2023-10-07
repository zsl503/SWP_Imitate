#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#define FRAME_PAYLOAD_SIZE 48
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
int main() {
    printf("%d\n",sizeof(struct Frame_t));
    return 0;
}
