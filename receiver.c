#include "receiver.h" // receiver.h

void init_receiver(Receiver *receiver,
                   int id)
{
    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;
    receiver->window_size = 4;
    receiver->next_seq = 0;
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
            fprintf(stderr, "\t<RECV-%d>:Recv corrupted msg\n", receiver->recv_id);
            free(inframe);
            return;
        }

        if (inframe->dst_port != receiver->recv_id)
        {
            fprintf(stderr, "\t<RECV-%d>:Recv msg for others\n", receiver->recv_id);
            free(inframe);
            return;
        }

        inframe->seq_num %= MAX_SEQ_NUM;
        uint8_t border = (receiver->base + receiver->window_size) % MAX_SEQ_NUM;

        if ((receiver->base <= border && inframe->seq_num >= receiver->base && inframe->seq_num < border) ||
            (receiver->base > border && (inframe->seq_num >= receiver->base || inframe->seq_num < border)))
        {
            Frame *outgoing_frame = (Frame *)malloc(sizeof(Frame));
            memset((void *)outgoing_frame, 0, sizeof(Frame));
            outgoing_frame->src_port = receiver->recv_id;
            outgoing_frame->dst_port = inframe->src_port;
            outgoing_frame->seq_num = 0;
            outgoing_frame->window_size = receiver->window_size;

            int pos = inframe->seq_num % receiver->window_size;
            receiver->recv_state |= (1 << pos); // 对应位置1
            if (inframe->seq_num == receiver->next_seq)
            {
                uint8_t mask = 0xFF >> (8 - receiver->window_size);
                uint8_t state = receiver->recv_state & mask;
                if (state == mask)
                {
                    receiver->recv_state = 0;
                    receiver->next_seq = (receiver->base + receiver->window_size) % MAX_SEQ_NUM;
                    receiver->base = (receiver->base + receiver->window_size) % MAX_SEQ_NUM;
                }
                else
                {
                    // 从recv_state中获取第一个不为0的值
                    int biggest_ack = 0;
                    while (state & 1)
                    {
                        biggest_ack++;
                        state >>= 1;
                    }
                    receiver->next_seq = (receiver->base + biggest_ack) % MAX_SEQ_NUM;
                }
                
                // 输出接收到的消息
                int i, end_pos = (receiver->next_seq > 0 ? receiver->next_seq - 1: MAX_SEQ_NUM - 1)% receiver->window_size;
                for(i = pos;i < end_pos;i++)
                {
                    printf("<RECV-%d>:[%s]\n", receiver->recv_id, receiver->recv_frames[i].data);
                }
                printf("<RECV-%d>:[%s]\n", receiver->recv_id, inframe->data);

            }
            outgoing_frame->ack_num = receiver->next_seq > 0 ? receiver->next_seq - 1: MAX_SEQ_NUM - 1;
            // 最后计算crc
            outgoing_frame->crc = compute_crc(outgoing_frame);
            ll_append_node(outgoing_frames_head_ptr, (void *)outgoing_frame);
            memcpy(&receiver->recv_frames[pos], outgoing_frame, sizeof(Frame));
            fprintf(stderr, "\t<RECV-%d>: Received Seq %d ,back ack's crc %d\n", receiver->recv_id, inframe->seq_num, outgoing_frame->crc);
        }
        else
        {
            Frame *outgoing_frame = (Frame *)malloc(sizeof(Frame));
            memset((void *)outgoing_frame, 0, sizeof(Frame));
            outgoing_frame->src_port = receiver->recv_id;
            outgoing_frame->dst_port = inframe->src_port;
            outgoing_frame->seq_num = 0;
            outgoing_frame->window_size = receiver->window_size;
            outgoing_frame->ack_num = receiver->next_seq > 0 ? receiver->next_seq - 1: MAX_SEQ_NUM - 1;
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
