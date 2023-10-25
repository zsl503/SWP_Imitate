#include "receiver.h" // receiver.h

#define WINDOW_ADD(x, y, z) ((x + y) % z)
#define WINDOW_SUB(x, y, z) ((x + z - y) % z)
#define SEQ_ADD(x, y) ((x + y) % MAX_SEQ_NUM)
#define SEQ_SUB(x, y) ((x + MAX_SEQ_NUM - y) % MAX_SEQ_NUM)

void init_receiver(Receiver *receiver,
                   int id)
{
    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;
    receiver->receiver_one = (struct Receiver_one *)malloc(sizeof(struct Receiver_one) * glb_senders_array_length);

    int i = 0;
    for (; i < glb_senders_array_length; i++)
    {
        receiver->receiver_one[i].window_size = 8;
    }
}

void handle_incoming_msgs(Receiver *receiver,
                          LLnode **outgoing_frames_head_ptr)
{
    // TODO: Suggested steps for handling incoming frames
    //     1) Dequeue the Frame from the sender->input_framelist_head
    //     2) Convert the char * buffer to a Frame data type
    //     3) Check whether the frame is corrupted
    //     4) Check whether the frame is for this receiver
    //     5) Do sliding window protocol for sender/receiver pair

    int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
    while (incoming_msgs_length > 0)
    {
        // Pop a node off the front of the link list and update the count
        LLnode *ll_inmsg_node = ll_pop_node(&receiver->input_framelist_head);
        incoming_msgs_length = ll_get_length(receiver->input_framelist_head);

        // DUMMY CODE: Print the raw_char_buf
        // NOTE: You should not blindly print messages!
        //       Ask yourself: Is this message really for me?
        //                     Is this message corrupted?
        //                     Is this an old, retransmitted message?
        Frame *inframe = convert_char_to_frame((char *)ll_inmsg_node->value);
        ll_destroy_node(ll_inmsg_node);

        if (is_corrupted(inframe))
        {
            // Corrupted frame, ignore it
            fprintf(stderr, "\t<RECV-%d>:Recv corrupted msg.\n", receiver->recv_id);
            free(inframe);
            return;
        }

        if ((inframe->state | 0b00000001) == 0)
        {
            fprintf(stderr, "<RECV-%d>:Unused seq.\n", receiver->recv_id);
            free(inframe);
            return;
        }

        if (inframe->dst_id != receiver->recv_id)
        {
            fprintf(stderr, "\t<RECV-%d>:Recv msg for others\n", receiver->recv_id);
            free(inframe);
            return;
        }

        inframe->seq_num %= MAX_SEQ_NUM;
        uint8_t src_id = inframe->src_id;
        struct Receiver_one *recv_one = &receiver->receiver_one[src_id];
        int len = SEQ_SUB(inframe->seq_num, recv_one->base);
        if (len < recv_one->window_size)
        {
            int pos = WINDOW_ADD(recv_one->window_base, len, recv_one->window_size);
            Frame *outgoing_frame = (Frame *)malloc(sizeof(Frame));
            memset((void *)outgoing_frame, 0, sizeof(Frame));
            outgoing_frame->src_id = receiver->recv_id;
            outgoing_frame->dst_id = inframe->src_id;
            outgoing_frame->window_size = recv_one->window_size;

            memcpy(&recv_one->recv_frames[pos], inframe, sizeof(Frame));
            recv_one->recv_state |= (1 << pos); // 对应位置1

            if (len == 0) // 移动窗口
            {
                uint8_t mask = 0xFF >> (8 - recv_one->window_size);
                uint8_t state = recv_one->recv_state & mask;
                if (state == mask) // 满了
                {
                    recv_one->recv_state = 0;
                    recv_one->base = SEQ_ADD(recv_one->base, recv_one->window_size);
                }
                else
                {
                    uint8_t wb_tmp = recv_one->window_base;
                    uint8_t tmp = 1 << recv_one->window_base;
                    while (tmp)
                    {
                        if (tmp & state) // 等于1说明已经收到了，否则未收到
                        {
                            recv_one->recv_state &= ~tmp; // 置0
                            recv_one->base = SEQ_ADD(recv_one->base, 1);
                            recv_one->window_base = WINDOW_ADD(recv_one->window_base, 1, recv_one->window_size);
                        }
                        else
                        {
                            wb_tmp = 0; // 不需要下面从头开始的查找了
                            break;
                        }
                        tmp <<= 1;
                    }
                    tmp = 1;
                    while (wb_tmp)
                    {
                        if (tmp & state) // 等于1说明已经收到了，否则未收到
                        {
                            recv_one->recv_state &= ~tmp; // 置0
                            recv_one->base = SEQ_ADD(recv_one->base, 1);
                            recv_one->window_base = WINDOW_ADD(recv_one->window_base, 1, recv_one->window_size);
                        }
                        else
                            break;
                        tmp <<= 1;
                    }
                }
                int i, len = WINDOW_SUB(recv_one->window_base, pos, recv_one->window_size);
                if(len == 0) len = recv_one->window_size;
                for (i = pos; i < pos + len; i++)
                {
                    fprintf(stderr, "<RECV-%d>:[%s]\n", receiver->recv_id, recv_one->recv_frames[i%recv_one->window_size].data);
                }
                for (i = pos; i < pos + len; i++)
                {
                    printf("<RECV-%d>:[%s]\n", receiver->recv_id, recv_one->recv_frames[i%recv_one->window_size].data);
                }
                fprintf(stderr, "\t<Out-windows %d> pos:%d end_pos:%d, len:%d\n", receiver->recv_id, pos, recv_one->window_base, len);
                for (i = 0; i < 8; i++)
                {
                    fprintf(stderr, "\t<Out-windows %d> %d:[%s]\n", receiver->recv_id, i, recv_one->recv_frames[i].data);
                }
            }

            // 输出接收到的消息

            outgoing_frame->state = 0b00000010;
            outgoing_frame->ack_num = SEQ_SUB(recv_one->base, 1);
            // 最后计算crc
            outgoing_frame->crc = compute_crc(outgoing_frame);
            ll_append_node(outgoing_frames_head_ptr, (void *)outgoing_frame);
            fprintf(stderr, "\t<RECV-%d>: Received Seq %d ,crc %d, back ack's crc %d\n", receiver->recv_id, inframe->seq_num, inframe->crc, outgoing_frame->crc);
        }
        else
        {
            Frame *outgoing_frame = (Frame *)malloc(sizeof(Frame));
            memset((void *)outgoing_frame, 0, sizeof(Frame));
            outgoing_frame->src_id = receiver->recv_id;
            outgoing_frame->dst_id = inframe->src_id;
            outgoing_frame->window_size = recv_one->window_size;
            outgoing_frame->state = 0b00000010;
            outgoing_frame->ack_num = SEQ_SUB(recv_one->base, 1);
            outgoing_frame->crc = compute_crc(outgoing_frame);
            ll_append_node(outgoing_frames_head_ptr, (void *)outgoing_frame);
            fprintf(stderr, "\t<RECV-%d>:Outside Seq %d\n", receiver->recv_id, inframe->seq_num);
        }

        free(inframe);
    }
}

void *run_receiver(void *input_receiver)
{
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Receiver *receiver = (Receiver *)input_receiver;
    LLnode *outgoing_frames_head;

    // This incomplete receiver thread, at a high level, loops as follows:
    // 1. Determine the next time the thread should wake up if there is nothing in the incoming queue(s)
    // 2. Grab the mutex protecting the input_msg queue
    // 3. Dequeues messages from the input_msg queue and prints them
    // 4. Releases the lock
    // 5. Sends out any outgoing messages

    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);

    while (1)
    {
        // NOTE: Add outgoing messages to the outgoing_frames_head pointer
        outgoing_frames_head = NULL;
        gettimeofday(&curr_timeval,
                     NULL);

        // Either timeout or get woken up because you've received a datagram
        // NOTE: You don't really need to do anything here, but it might be useful for debugging purposes to have the receivers periodically wakeup and print info
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        if (time_spec.tv_nsec >= 1000000000)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input frames should go
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&receiver->buffer_mutex);

        // Check whether anything arrived
        int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
        if (incoming_msgs_length == 0)
        {
            // Nothing has arrived, do a timed wait on the condition variable (which releases the mutex). Again, you don't really need to do the timed wait.
            // A signal on the condition variable will wake up the thread and reacquire the lock
            pthread_cond_timedwait(&receiver->buffer_cv,
                                   &receiver->buffer_mutex,
                                   &time_spec);
        }

        handle_incoming_msgs(receiver,
                             &outgoing_frames_head);

        pthread_mutex_unlock(&receiver->buffer_mutex);

        // CHANGE THIS AT YOUR OWN RISK!
        // Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while (ll_outgoing_frame_length > 0)
        {
            LLnode *ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char *char_buf = convert_frame_to_char((Frame *)ll_outframe_node->value);

            // The following function frees the memory for the char_buf object
            send_msg_to_senders(char_buf);

            // Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
}
