/*******************************************************************
*
*    DESCRIPTION: sip in ap
*
*    AUTHOR: zouzhenhua
*
*    HISTORY: 2.0        sendzou( config.sip_socket, &sip_msg, SIP_SIZE, 0 );
*
*    DATE:3/7/2007
*
*******************************************************************/

#include "../../src/func.h"
#include "../../src/main.h"

#define TIMEOUT 500000

/**
 * global configuraion struct
 */
cfg_t config;

/**
 * global serial message
 */
SerialMSG_t serial_msg;

/**
 * global sip socket message
 */
SipMSG_t sip_msg;

/**
 * another message type
 * it means the sip_serial_txd and serial_sip_txd functions
 * can receive any type of message
 * and return the type of the received message
 */
#define arbitrary 0xFFFFFFFF

/**
 * build and transmit err message to sip socket
 */
void inline sip_err()
{
    sip_msg.msg_type = ERROR;
    sip_msg.sdr_id = config.sdr_id;
    sip_msg.ap_id = config.ap_id;
    socklen_t len = sizeof(struct sockaddr_in);
    sendto(config.sip_socket, &sip_msg, SIP_SIZE, 0, (struct sockaddr*)&config.pc_addr, len);
}

/**
 * build and transmit err message to serial port
 */
void inline serial_err()
{
    serial_msg.msg_type = (unsigned char)ERROR;
    serial_msg.sdr_id = config.sdr_id;
    serial_msg.ap_id = config.ap_id;
    serial_txd(config.serial_control, serial_msg);
}

/**
 * it implement the left part of the flow chart
 * the pc initializes the audio session
 *
 * @return 0 if OK, -1 if ERROR
 */
int sip_loop()
{
    SipMSG_Type_t i;
    struct timeval time_stamp;

    if (sip_serial_txd(INVITE, TIMEOUT) != INVITE)
        return -1;

    config.sdr_id = serial_msg.sdr_id;

    gettimeofday(&time_stamp, NULL);
    sip_msg.time_stamp = (long)time_stamp.tv_sec;
    /* if it did not recevie register message from serial in 500ms, then transmit error to sip */
    if (serial_sip_txd((char)REGISTER, TIMEOUT) != (char)REGISTER)
    {
        sip_err();
        return -1;
    }

    /* transmit channel message to voice serial port */
    //serial_txd(config.serial_voice, serial_msg);

    /* the legal incoming socket message can be ERROR or OK, so the specified message type is arbitrary */
    i = sip_serial_txd(arbitrary, TIMEOUT);
    if (i == ERROR)
    {
        serial_err();
        return -1;
    }
    else if (i == RING)
    {
        serial_txd(config.serial_control, serial_msg);
        config.pc_rtp_port = sip_msg.pc_rtp_port;
    }
    else
    {
        sip_err();
        serial_err();
        return -1;
    }

    /* expecting RING message from serial in 500ms */
    /*    if (serial_sip_txd((unsigned char)RING, TIMEOUT) != (unsigned char)RING) {
            sip_err();
            return -1;
        }*/

    /* open thread to transmit ring audio */
    config.ring_lock = FREE;
    pthread_t threadId;
    pthread_create(&threadId, NULL, ring_txd, NULL);

    sip_msg.ap_rtp_port = config.ap_rtp_port;
    if (serial_sip_txd((unsigned char)OK, 3000000) != (unsigned char)OK)
    {
        sip_err();
        config.ring_lock = USED; /* close the ring transmission thread */
        return -1;
    }

    config.ring_lock = USED; /* close the ring transmission thread */

    /* the handshake procedure is sucessfully */
    return 0;

}

/**
 * it implement the right part of the flow chart
 * the sdr initializes the audio session
 *
 * @return 0 if OK, -1 if ERROR
 */
int serial_loop()
{
    unsigned char i;
    SipMSG_Type_t j;
    socklen_t len = sizeof(struct sockaddr_in);

    /* the legal message can be TRANSFER and REGISTER message */
    i = serial_sip_txd((unsigned char)arbitrary, TIMEOUT);
    if (i == (unsigned char)TRANSFER)
    {             /* transfer */
        config.sdr_id = sip_msg.sdr_id;
        sip_msg.ap_id = config.ap_id;
        sendto(config.sip_socket, &sip_msg, SIP_SIZE, 0, (struct sockaddr*)&config.pc_addr, len);
        serial_txd(config.serial_voice, serial_msg); /* transmit channel message to voice serial port */

        /* the legal message can be OK and ERROR message */
        j = sip_serial_txd(arbitrary, TIMEOUT);
        if (j == ERROR)
            return -1;
        else if (j != OK)
        { /* other message received, the transmit err to sip */
            sip_err();
            return -1;
        }
        /* everything is ok, then move to rtp session */
    }
    else if (i == (unsigned char)REGISTER)
    { /* register */
        config.sdr_id = sip_msg.sdr_id;

        struct timeval time_stamp;
        gettimeofday(&time_stamp, NULL);
        sip_msg.time_stamp = (long)time_stamp.tv_sec;

        sip_msg.ap_id = config.ap_id;

        if (SIP_SIZE != sendto(config.sip_socket, &sip_msg, SIP_SIZE, 0, (struct sockaddr*)&config.pc_addr, len))
            printf("send error!\n");

        // serial_txd(config.serial_voice, serial_msg); /* transmit channel message to voice serial port */

        j = sip_serial_txd(arbitrary, TIMEOUT);
        if (j == ERROR)
        {
            serial_err();
            return -1;
        }
        else if (j == RING)
        {
            serial_txd(config.serial_control, serial_msg);
        }
        else
        {
            sip_err();
            serial_err();
            return -1;
        }

        /* open ring transmisson thread */
        config.ring_lock = FREE;
        pthread_t threadId;
        pthread_create(&threadId, NULL, ring_txd, NULL);

        if (sip_serial_txd(OK, 30000) != OK)
        {
            serial_err();
            config.ring_lock = USED; /* close the ring transmission thread */
            return -1;
        }

        config.ring_lock = USED; /* close the ring transmission thread */
    }
    else
        return -1;

    config.pc_rtp_port = sip_msg.pc_rtp_port;

    return 0;
}

pthread_mutex_t lock;
int receive_write_fd;
int receive_read_fd;
int send_write_fd;
int send_read_fd;
int bye_notified = 0;
int rssi_notified = 0;

/* use global message serial_msg */
void* receive_serial(void* arg)
{
    int status = 0;
    int i = 0;
    int length = 0;

    //int fd = open("test", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    while(1)
    {
        while (1)
        {
            pthread_mutex_lock(&lock);
            status = serial_rxd(config.serial_voice, &serial_msg);
            pthread_mutex_unlock(&lock);
            if (status != 0)
            {
                usleep(10000);
                continue;
            }
            if (serial_msg.data_length != 120)
            {
                usleep(10000);
                continue;
            }
            break;
        }

        /* receive bye message from serial */
        if (serial_msg.msg_type == 0x05)
        {
            bye_notified = 1;
            break;
        }

        /* check rssi value */
        if (rssi_compare(serial_msg.rssi) == 1)
        {

            rssi_notified = 1;
            break;
        }

        length = write(receive_write_fd, serial_msg.data, 120);
        if (length != 120)
        {
            printf("write receive_write_fd fatal error!\n");
            printf("length is %d\n", length);
        }
        //         printf("serial receive sucess!\n");
    }
}

/* use locally allocated msg snd_serial_msg*/
void* send_serial(void* arg)
{
    //     struct timeval old_time;
    //     struct timeval cur_time;
    //     long usec = 0;

    int j = 0;
    int i = 0;
    int length = 0;
    int flag = 0;
    SerialMSG_t snd_serial_msg;
    snd_serial_msg.begin_syn = 0x55;
    snd_serial_msg.sdr_id = config.sdr_id;
    snd_serial_msg.ap_id = config.ap_id;
    snd_serial_msg.msg_type = 0xEE;
    snd_serial_msg.channel = 0x02;
    snd_serial_msg.data_length = VOICE_LENGTH;
    snd_serial_msg.rssi = 0x00;
    snd_serial_msg.CRC = 0x00;
    snd_serial_msg.end_syn = 0x55;
    unsigned char buffer[VOICE_LENGTH];
    snd_serial_msg.data = buffer;

    //     gettimeofday(&old_time, 0);
    while (1)
    {
        while (1)
        {
            length = read(send_read_fd, snd_serial_msg.data, 120);
            if (length != 120)
            {
                usleep(10000);
                continue;
            }
            break;
        }

        pthread_mutex_lock(&lock);
        if (0 != serial_txd(config.serial_voice, snd_serial_msg))
            printf("send to serial error!\n");
        pthread_mutex_unlock(&lock);

        //         usec = 0;
        //         while (1)
        //         {
        //             gettimeofday(&cur_time, 0);
        //             /* calcuate the time difference in mircoseconds & store it to usec variable */
        //             if (cur_time.tv_sec - old_time.tv_sec > 0)
        //                 usec = cur_time.tv_usec + 1000000 * (cur_time.tv_sec - old_time.tv_sec);
        //             else
        //                 usec = cur_time.tv_usec;
        //
        //             /* if difference is larger than 60ms then transmit the data
        //             * and if the difference is smaller than 50ms then sleep 10ms */
        //             if (usec - old_time.tv_usec >= 60000)
        //                 break;
        //             else if (usec - old_time.tv_usec <= 50000)
        //                 usleep(10000);
        //         }
        //         old_time = cur_time;
        //         printf("serial send sucess!\n");
    }
    return ((void *)0);
}

int rtp()
{
    int pipe_fd[2];
    rssi_notified = 0;
    bye_notified = 0;
    char *buffer;
    buffer = (char*)malloc(120);
    if (buffer == NULL)
        return -1;

    if (pipe(pipe_fd) < 0)
        printf("pipe error!\n");
    send_read_fd = pipe_fd[0];
    send_write_fd = pipe_fd[1];
    if (pipe(pipe_fd) < 0)
        printf("pipe error!\n");
    receive_read_fd = pipe_fd[0];
    receive_write_fd = pipe_fd[1];
    if (receive_write_fd < 0 || receive_read_fd < 0 || send_read_fd < 0 || send_write_fd < 0)
    {
        printf("write fd error!\n");
        return -1;
    }

    //     struct termios Opt;
    //     tcgetattr(config.serial_voice, &Opt);
    //     //     fcntl(fd, F_SETFL, FNDELAY);
    //     fcntl(config.serial_voice, F_SETFL, 0);
    //     Opt.c_cc[VMIN] = 120;
    //     Opt.c_cc[VTIME] = 10;
    //     if (tcsetattr(config.serial_voice, TCSANOW, &Opt) != 0)
    //     {
    //         printf("set parity mode error!\n");
    //         exit(1);
    //     }

    struct sockaddr_in pc_receive_addr;
    struct sockaddr_in pc_transmit_addr;
    socklen_t len = sizeof(pc_receive_addr);
    if (rtp_init((struct sockaddr_in*)&pc_transmit_addr) != 0)
    {
        printf("initialize rtp error!\n");
        return -1;
    }

    int status;
    int recv_len;

    /* set sip socket to non block mode */
    //    fcntl(config.sip_socket, F_SETFL, O_NONBLOCK);
    fcntl(config.rtp_socket, F_SETFL, O_NONBLOCK);
    fcntl(receive_read_fd, F_SETFL, O_NONBLOCK);
    fcntl(send_read_fd, F_SETFL, O_NONBLOCK);
    fcntl(config.sip_socket, F_SETFL, O_NONBLOCK);
    //    fcntl(receive_read_fd, F_SETFL, 0);
    //    fcntl(send_read_fd, F_SETFL, 0);

    pthread_mutex_init(&lock , NULL);
    pthread_t send_serial_id;
    pthread_t receive_serial_id;
    pthread_create(&send_serial_id, NULL, send_serial, NULL);
    pthread_create(&receive_serial_id, NULL, receive_serial, NULL);

    //    int net_fd = open("net", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    while(1)
    {
        /* network receive*/
        if (120 == recvfrom(config.rtp_socket, buffer, 120, 0, (struct sockaddr*)&pc_receive_addr, &len))
            if (120 != write(send_write_fd, buffer, 120))
                return -1;

        /* serial receive */
        if (120 == read(receive_read_fd, buffer, 120))
        {
            if (120 != sendto(config.rtp_socket, buffer, 120, 0, (struct sockaddr*)&pc_transmit_addr, len))
                return -1;

        }

        if (rssi_notified == 1)
        {
            /* transmit transfer message */
            transfer_notify();
            break;
        }

        /* ----- check if sip socket comes in new message */
        if (recvfrom(config.sip_socket, (char *)&sip_msg, SIP_SIZE, 0, (struct sockaddr*)&pc_receive_addr, &len) != -1)
        {
            if (sip_msg.msg_type == BYE)
            {
                sip_serial_cpy();
                serial_txd(config.serial_voice, serial_msg);

                if (serial_sip_txd((unsigned char)OK, TIMEOUT) != (unsigned char)OK)
                    sip_err();
                break;
            }
        }

        /* receive bye message, then transmit bye and wait for sip socket OK message */
        if (bye_notified == 1)
        {
            serial_sip_cpy();
            socklen_t len;
            sendto(config.sip_socket, &sip_msg, SIP_SIZE, 0, (struct sockaddr*)&config.pc_addr, len);
            if (sip_serial_txd(OK, TIMEOUT) != OK )  /* if it does not receiv OK message */
                serial_err();
            break;
        }
    }

    free(buffer);
    rtp_close();
    return 0;
}

static void cleanup(int signo)
{
    if (signo == SIGINT)
    {
        free(serial_msg.data);
        free(config.serial_port_name[0]);
        free(config.serial_port_name[1]);
        free(config.ring_name);
        free(config.ap_ip);
        free(config.pc_ip);
        close(config.rtp_socket);
        close(config.sip_socket);
        close(config.serial_voice);
        close(config.serial_control);
        printf("clean up sucess!\n");
    }

    exit(1);
}

int main()
{
    memset(&config, 0, sizeof(cfg_t));
    /* main configuration default value */
    config.serial_port_name[1] = (char *)malloc(30); /* control serial port name */
    config.serial_port_name[0] = (char *)malloc(30); /* audio serial port name */
    strcpy(config.serial_port_name[1], "/dev/ttyUSB0"); /* voice */
    strcpy(config.serial_port_name[0], "/dev/ttyS0"); /* control */
    config.ring_name = (char *)malloc(100);
    strcpy(config.ring_name, "ring");
    config.ap_ip = (char *)malloc(30);
    config.pc_ip = (char *)malloc(30);
    strcpy(config.ap_ip, "172.21.134.120");
    strcpy(config.pc_ip, "172.21.134.113");
    config.ap_id = 0x5678;
    config.sdr_id = 0x0000;
    config.ring_lock = FREE;
    config.ap_rtp_port = 10500;
    config.pc_rtp_port = 0;
    config.rssi = 0x00;
    config.sip_socket = 0;
    config.rtp_socket = 0;

    //     struct timeval time_stamp;
    //     gettimeofday(&time_stamp, NULL);
    //     sip_msg.time_stamp = (long)time_stamp.tv_sec;

    unsigned char* serial_msg_temp;
    serial_msg.data = (unsigned char*)malloc(VOICE_LENGTH);

    if (serial_init() != 0)
    {
        printf( "initialize serial error!\n" );
        return -1;
    }

    if (sip_init() != 0)
    {
        printf("initialize sip error!\n");
        return -1;
    }

    // signal hanlder
    if (signal(SIGINT, cleanup) == SIG_ERR)
        printf("can't catch SIGINT!\n");

    //     if (1)
    //     {
    //         void *t;
    //         config.ring_lock = FREE;
    //         ring_txd(t);
    //         usleep(1000000);
    //         config.ring_lock = USED;
    //     }

    //    while(1) {usleep(1000000000);};

    //     socklen_t len = sizeof(struct sockaddr_in);
    //     sip_msg.msg_type = TRANSFER;
    //     if (sendto(config.sip_socket, &sip_msg, SIP_SIZE, 0, (struct sockaddr*)&config.pc_addr, len) != SIP_SIZE)
    //         return -1;

    //     fd_set input;
    //     int max_fd;
    //     FD_ZERO(&input);
    //     FD_SET(config.sip_socket, &input);
    //     FD_SET(config.serial_control, &input);
    //     max_fd = (config.sip_socket > config.serial_control ? config.sip_socket : config.serial_control) + 1;

    //     serial_msg.begin_syn = 0x55;
    //     serial_msg.sdr_id = 0x01;
    //     serial_msg.ap_id = 0x5678;
    //     serial_msg.msg_type = 0x00;
    //     serial_msg.channel = 0x00;
    //     serial_msg.data_length = 0x00;
    //     serial_msg.rssi = 0x00;
    //     serial_msg.CRC = 0x00;
    //     serial_msg.end_syn = 0x55;
    //     if (-1 == serial_txd(config.serial_control, serial_msg))
    //         perror("error");

    /*    while (1)
        {*/
    //         select(max_fd, &input, NULL, NULL, NULL);
    //
    //         if (FD_ISSET(config.sip_socket, &input))
    //         {
    //    if (sip_loop() == 0)   /* the function runs sucessfully, then open rtp */
        rtp();
    //         }

    //         if (FD_ISSET(config.serial_control, &input))
    //         {
    //if (serial_loop() == 0 )   /* the function runs sucessfully, then open rtp */
    //        rtp();
    //}
    //     }

    /* clean some configuration and message */
    //     config.pc_rtp_port = 0;
    //     config.ring_lock = FREE;
    //     config.rssi = 0;
    //     config.rtp_socket = 0;
    //     config.sdr_id = 0;
    //
    //     serial_msg_temp = serial_msg.data;
    //     memset(&serial_msg_temp, 0, SERIAL_SIZE);
    //     serial_msg.data = serial_msg_temp;
    //     serial_msg_temp = NULL;
    //     memset(&sip_msg, 0, SIP_SIZE);


    //
    //     if (1)
    //     {
    //         serial_msg.ap_id = 0x5678;
    //         serial_msg.begin_syn = 0x55;
    //         serial_msg.channel = 0x02;
    //         serial_msg.CRC = 0x00;
    //         serial_msg.data_length = 100;
    //         serial_msg.end_syn = 0x55;
    //         serial_msg.msg_type = 0xee;
    //         serial_msg.rssi = 0x00;
    //         serial_msg.sdr_id = 0x01;
    //         config.pc_rtp_port = 10000;
    //
    //         strcpy(config.pc_ip, "172.21.134.101");
    //         rtp();
    //     }

    free(serial_msg.data);
    free(config.serial_port_name[0]);
    free(config.serial_port_name[1]);
    free(config.ring_name);
    free(config.ap_ip);
    free(config.pc_ip);
    close(config.rtp_socket);
    close(config.sip_socket);
    close(config.serial_voice);
    close(config.serial_control);
    return 0;
}
