#include "sender.h" // sender.h

void init_sender(Sender *sender, int id)
{
    // TODO: You should fill in this function as necessary
    sender->send_id = id;
    sender->input_cmdlist_head = NULL;
    sender->input_framelist_head = NULL;

    sender->base = 0;
    sender->next_seq_num = 0;
    sender->window_size = 10;
}

struct timeval *sender_get_next_expiring_timeval(Sender *sender)
{
    // TODO: You should fill in this function so that it returns the next timeout that should occur
    return NULL;
}

void handle_incoming_acks(Sender *sender,
                          LLnode **outgoing_frames_head_ptr)
{
    // TODO: Suggested steps for handling incoming ACKs
    //     1) Dequeue the ACK from the sender->input_framelist_head
    //     2) Convert the char * buffer to a Frame data type
    //     3) Check whether the frame is corrupted
    //     4) Check whether the frame is for this sender
    //     5) Do sliding window protocol for sender/receiver pair

    // Dequeue the ACK from the sender->input_framelist_head
    LLnode *ack_node = ll_pop_node(&sender->input_framelist_head);
    if (ack_node == NULL)
    {
        // No ACKs to handle
        return;
    }

    // Convert the char * buffer to a Frame data type
    Frame *ack_frame = (Frame *)ack_node->value;
    free(ack_node);

    // Check whether the frame is corrupted
    // Check whether the frame is for this sender
    // Do sliding window protocol for sender/receiver pair
    if (is_corrupted(ack_frame) || ack_frame->dst_port != sender->send_id || ack_frame->ack_num < sender->base - 1)
    {
        // Corrupted frame, ignore it
        free(ack_frame);
        return;
    }

    // printf("<SEND_%d>: received ACK %d\n", sender->send_id, ack_frame->ack_num);

    // 如果base - 1 == ack，意味着收到重复ack，重传
    if (ack_frame->ack_num == sender->base - 1)
    {
        // ACK is for the base frame
        sender->next_seq_num = sender->base;
    }
    else if (ack_frame->ack_num >= sender->base && ack_frame->ack_num < sender->next_seq_num)
    {
        sender->base = (ack_frame->ack_num + 1) % 128;
    }
    else
    {
        // 非法ack值，输出错误信息
        printf("<SEND_%d>: received an invalid ACK value %d\n", sender->send_id, ack_frame->ack_num);
    }
}

void handle_input_cmds(Sender *sender,
                       LLnode **outgoing_frames_head_ptr)
{
    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);

    // Recheck the command queue length to see if stdin_thread dumped a command on us
    while (input_cmd_length > 0 && sender->next_seq_num < sender->base + sender->window_size)
    {
        // Pop a node off and update the input_cmd_length
        LLnode *ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        // Cast to Cmd type and free up the memory for the node
        Cmd *outgoing_cmd = (Cmd *)ll_input_cmd_node->value;
        int msg_length = strlen(outgoing_cmd->message);

        // Split the message into multiple frames if necessary
        char *msg_ptr = outgoing_cmd->message;
        int remaining_bytes = msg_length;

        while (remaining_bytes > 0)
        {
            // Compute the length of the payload for this frame
            int payload_length = remaining_bytes > FRAME_PAYLOAD_SIZE ? FRAME_PAYLOAD_SIZE : remaining_bytes;

            // Create a new frame
            Frame *outgoing_frame = (Frame *)malloc(sizeof(Frame));
            memset((void *)outgoing_frame, 0, sizeof(Frame));
            outgoing_frame->src_port = sender->send_id;
            outgoing_frame->dst_port = outgoing_cmd->dst_id;
            outgoing_frame->seq_num = sender->next_seq_num;
            outgoing_frame->window_size = sender->window_size;
            memcpy(outgoing_frame->data, msg_ptr, payload_length);

            // Compute CRC and add to frame
            outgoing_frame->crc = compute_crc(outgoing_frame);

            ll_append_node(outgoing_frames_head_ptr, (void * )outgoing_frame);

            // Update sender state
            sender->next_seq_num++;
            remaining_bytes -= payload_length;
            msg_ptr += payload_length;
        }
        // Free the outgoing command
        ll_destroy_node(ll_input_cmd_node);
    }
}

void handle_timedout_frames(Sender *sender,
                            LLnode **outgoing_frames_head_ptr)
{
    // TODO: Suggested steps for handling timed out datagrams
    //     1) Iterate through the sliding window protocol information you maintain for each receiver
    //     2) Locate frames that are timed out and add them to the outgoing frames
    //     3) Update the next timeout field on the outgoing frames
}

void *run_sender(void *input_sender)
{
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender *sender = (Sender *)input_sender;
    LLnode *outgoing_frames_head;
    struct timeval *expiring_timeval;
    long sleep_usec_time, sleep_sec_time;

    // This incomplete sender thread, at a high level, loops as follows:
    // 1. Determine the next time the thread should wake up
    // 2. Grab the mutex protecting the input_cmd/inframe queues
    // 3. Dequeues messages from the input queue and adds them to the outgoing_frames list
    // 4. Releases the lock
    // 5. Sends out the messages

    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);
    while (1)
    {
        outgoing_frames_head = NULL;

        // Get the current time
        gettimeofday(&curr_timeval,
                     NULL);

        // time_spec is a data structure used to specify when the thread should wake up
        // The time is specified as an ABSOLUTE (meaning, conceptually, you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        // Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender);

        // Perform full on timeout
        if (expiring_timeval == NULL)
        {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        }
        else
        {
            // Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval,
                                               expiring_timeval);

            // Sleep if the difference is positive
            if (sleep_usec_time > 0)
            {
                sleep_sec_time = sleep_usec_time / 1000000;
                sleep_usec_time = sleep_usec_time % 1000000;
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time * 1000;
            }
        }

        // Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input frames or input commands should go
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        // Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);

        // Nothing (cmd nor incoming frame) has arrived, so do a timed wait on the sender's condition variable (releases lock)
        // A signal on the condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 &&
            inframe_queue_length == 0)
        {

            pthread_cond_timedwait(&sender->buffer_cv,
                                   &sender->buffer_mutex,
                                   &time_spec);
        }

        // Implement this   ack输入
        handle_incoming_acks(sender,
                             &outgoing_frames_head);

        // Implement this   端口输入
        handle_input_cmds(sender,
                          &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);

        // Implement this
        handle_timedout_frames(sender,
                               &outgoing_frames_head);

        // CHANGE THIS AT YOUR OWN RISK!
        // Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while (ll_outgoing_frame_length > 0)
        {
            LLnode *ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char *char_buf = convert_frame_to_char((Frame * )ll_outframe_node->value);

            // Don't worry about freeing the char_buf, the following function does that
            send_msg_to_receivers(char_buf);

            // Free up the ll_outframe_node
            ll_destroy_node(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}