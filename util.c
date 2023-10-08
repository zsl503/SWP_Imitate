#include "util.h"

int is_corrupted(Frame * frame)
{
    if(compute_crc(frame) != frame->crc)
        return 1;
    return 0;
}


uint16_t compute_crc(Frame * frame)
{
    uint16_t tmpcrc = frame->crc;
    frame->crc = 0;
    char * buffer = convert_frame_to_char(frame);
    uint16_t crc = 0xFFFF;
    uint8_t * data = (uint8_t *) buffer;
    int data_size = sizeof(char) * sizeof(buffer);
    int i, j;
    for (i = 0; i < data_size; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    free(buffer);
    frame->crc = tmpcrc;
    return crc;
}


//Linked list functions
int ll_get_length(LLnode * head)
{
    LLnode * tmp;
    int count = 1;
    if (head == NULL)
        return 0;
    else
    {
        tmp = head->next;
        while (tmp != head)
        {
            count++;
            tmp = tmp->next;
        }
        return count;
    }
}

void ll_append_node(LLnode ** head_ptr, 
                    void * value)
{
    LLnode * prev_last_node;
    LLnode * new_node;
    LLnode * head;

    if (head_ptr == NULL)
    {
        return;
    }
    
    //Init the value pntr
    head = (*head_ptr);
    new_node = (LLnode *) malloc(sizeof(LLnode));
    new_node->value = value;

    //The list is empty, no node is currently present
    if (head == NULL)
    {
        (*head_ptr) = new_node;
        new_node->prev = new_node;
        new_node->next = new_node;
    }
    else
    {
        //Node exists by itself
        prev_last_node = head->prev;
        head->prev = new_node;
        prev_last_node->next = new_node;
        new_node->next = head;
        new_node->prev = prev_last_node;
    }
}


LLnode * ll_pop_node(LLnode ** head_ptr)
{
    LLnode * last_node;
    LLnode * new_head;
    LLnode * prev_head;

    prev_head = (*head_ptr);
    if (prev_head == NULL)
    {
        return NULL;
    }
    last_node = prev_head->prev;
    new_head = prev_head->next;

    //We are about to set the head ptr to nothing because there is only one thing in list
    if (last_node == prev_head)
    {
        (*head_ptr) = NULL;
        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
    else
    {
        (*head_ptr) = new_head;
        last_node->next = new_head;
        new_head->prev = last_node;

        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
}

void ll_destroy_node(LLnode * node)
{
    if (node->type == llt_string)
    {
        free((char *) node->value);
    }
    free(node);
}

//Compute the difference in usec for two timeval objects
long timeval_usecdiff(struct timeval *start_time, 
                      struct timeval *finish_time)
{
  long usec;
  usec=(finish_time->tv_sec - start_time->tv_sec)*1000000;
  usec+=(finish_time->tv_usec- start_time->tv_usec);
  return usec;
}


//Print out messages entered by the user
void print_cmd(Cmd * cmd)
{
    fprintf(stderr, "src=%d, dst=%d, message=%s\n", 
           cmd->src_id,
           cmd->dst_id,
           cmd->message);
}


char * convert_frame_to_char(Frame * frame)
{
    char * char_buf = (char *) malloc(MAX_FRAME_SIZE);
    memset(char_buf, 0, MAX_FRAME_SIZE);

    memcpy(char_buf, &frame->src_port, sizeof(uint8_t));
    memcpy(char_buf + sizeof(uint8_t), &frame->dst_port, sizeof(uint8_t));
    memcpy(char_buf + 2 * sizeof(uint8_t), &frame->seq_num, sizeof(uint8_t));
    memcpy(char_buf + 3 * sizeof(uint8_t), &frame->ack_num, sizeof(uint8_t));
    memcpy(char_buf + 4 * sizeof(uint8_t), &frame->state, sizeof(uint8_t));
    memcpy(char_buf + 5 * sizeof(uint8_t), &frame->window_size, sizeof(uint8_t));
    // ext data
    memcpy(char_buf + 6 * sizeof(uint8_t), &frame->ext, sizeof(uint64_t));
    memcpy(char_buf + 14 * sizeof(uint8_t), &frame->crc, sizeof(uint16_t));
    memcpy(char_buf + 16 * sizeof(uint8_t), frame->data, sizeof(frame->data));

    return char_buf;
}

Frame * convert_char_to_frame(char * char_buf)
{
    //TODO: You should implement this as necessary
    Frame * frame = (Frame *) malloc(sizeof(Frame));

    memcpy(&frame->src_port, char_buf, sizeof(uint8_t));
    memcpy(&frame->dst_port, char_buf + sizeof(uint8_t), sizeof(uint8_t));
    memcpy(&frame->seq_num, char_buf + 2 * sizeof(uint8_t), sizeof(uint8_t));
    memcpy(&frame->ack_num, char_buf + 3 * sizeof(uint8_t), sizeof(uint8_t));
    memcpy(&frame->state, char_buf + 4 * sizeof(uint8_t), sizeof(uint8_t));
    memcpy(&frame->window_size, char_buf + 5 * sizeof(uint8_t), sizeof(uint8_t));
    // ext data
    memcpy(&frame->ext, char_buf + 6 * sizeof(uint8_t), sizeof(uint64_t));
    memcpy(&frame->crc, char_buf + 14 * sizeof(uint8_t), sizeof(uint16_t));
    memcpy(&frame->data, char_buf + 16 * sizeof(uint8_t), FRAME_PAYLOAD_SIZE);

    return frame;
}
