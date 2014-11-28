/*******************************************************************
*
*    DESCRIPTION: sip in ap
*
*    AUTHOR: zouzhenhua
*
*    HISTORY: 2.0
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

int sendzou(int fd, const SerialMSG_t* sip_msg, int length, int type)
{
    return length;
}

int recvzou(int fd, SerialMSG_t* sip_msg, int length, int type)
{
    if (read(fd, sip_msg, length) != length)
        return -1;
    return length;
}

/**
 * build and transmit err message to sip socket
 */
void inline sip_err() 
{
    sip_msg.msg_type = ERROR;
    sip_msg.sdr_id = config.sdr_id;
    sip_msg.ap_id = config.ap_id;
    sendzou(config.sip_socket, &sip_msg, SIP_SIZE, 0);
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

    if (sip_serial_txd(INVITE, TIMEOUT) != INVITE)
        return -1;

    config.sdr_id = serial_msg.sdr_id;

    /* if it did not recevie register message from serial in 500ms, then transmit error to sip */
    if (serial_sip_txd((char)REGISTER, TIMEOUT) != (char)REGISTER) {
        sip_err();
        return -1;
    }

    /* transmit channel message to voice serial port */
    serial_txd(config.serial_voice, serial_msg); 

    /* the legal incoming socket message can be ERROR or OK, so the specified message type is arbitrary */
    i = sip_serial_txd(arbitrary, TIMEOUT);
    if (i == ERROR) {
        serial_err();
        return -1;
    } else if (i == OK) {
        serial_txd(config.serial_control, serial_msg);
    } else {
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

    if (serial_sip_txd((unsigned char)OK, 30000) != (unsigned char)OK) {
        sip_err();
        config.ring_lock = USED; /* close the ring transmission thread */
        return -1;
    }

    config.ring_lock = USED; /* close the ring transmission thread */

    config.pc_rtp_port = sip_msg.pc_rtp_port;

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

    /* the legal message can be TRANSFER and REGISTER message */
    i = serial_sip_txd((unsigned char)arbitrary, TIMEOUT);
    if (i == (unsigned char)TRANSFER) {             /* transfer */
        config.sdr_id = sip_msg.sdr_id;
        sendzou(config.sip_socket, &sip_msg, SIP_SIZE, 0);
        serial_txd(config.serial_voice, serial_msg); /* transmit channel message to voice serial port */

        /* the legal message can be OK and ERROR message */
        j = sip_serial_txd(arbitrary, TIMEOUT);
        if (j == ERROR)
            return -1;
        else if (j != OK) {/* other message received, the transmit err to sip */
            sip_err(); 
            return -1;
        }
        /* everything is ok, then move to rtp session */
    } else if (i == (unsigned char)REGISTER) { /* register */
        config.sdr_id = sip_msg.sdr_id;
        sendzou(config.sip_socket, &sip_msg, SIP_SIZE, 0);
        serial_txd(config.serial_voice, serial_msg); /* transmit channel message to voice serial port */

        j = sip_serial_txd(arbitrary, TIMEOUT);
        if (j == ERROR) {
            serial_err();
            return -1;
        } else if (j == OK) {
            serial_txd(config.serial_control, serial_msg);
        } else {
            sip_err();
            serial_err();
            return -1;
        }

        if (sip_serial_txd(RING, TIMEOUT) != RING) {
            serial_err();
            return -1;
        }

        /* open ring transmisson thread */
        config.ring_lock = FREE;
        pthread_t threadId;
        pthread_create(&threadId, NULL, ring_txd, NULL);

        if (sip_serial_txd(OK, 30000) != OK) {
            serial_err();
            config.ring_lock = USED; /* close the ring transmission thread */
            return -1;
        }

        config.ring_lock = USED; /* close the ring transmission thread */

    } else
        return -1;

    config.pc_rtp_port = sip_msg.pc_rtp_port;

    return 0;
}

char *buf;
pthread_mutex_t lock;
int snd = 0;

void* send_serial(void* arg)
{
    struct timeval old_time;
    struct timeval cur_time;

    int flag = 0;

    int j = 0;
    while (1) {
        if (snd == 0 ) {
            usleep(5000);
            continue;
        }

        if (flag == 0) {
            gettimeofday(&old_time, 0);
            flag++;
        }

        long usec = 0;
        for (j = 0; j < 5; j++) {
            usec = 0;
            while (1) {
                gettimeofday(&cur_time, 0);
                /* calcuate the time difference in mircoseconds & store it to usec variable */
                if (cur_time.tv_sec - old_time.tv_sec > 0)
                    usec = cur_time.tv_usec+1000000*(cur_time.tv_sec - old_time.tv_sec);
                else
                    usec = cur_time.tv_usec;

                /* if difference is larger than 50ms then transmit the data 
                 * and if the difference is smaller than 40ms then sleep 10ms */
                if (usec - old_time.tv_usec >= 50000)
                    break;
                else if (usec - old_time.tv_usec <= 40000)
                    usleep(5000);
            }
            old_time = cur_time;

            strncpy(serial_msg.data, buf+j*100, 100);

            if (0 != serial_txd(config.serial_voice, serial_msg))
                printf("transmit error!\n");
        }
        pthread_mutex_lock(&lock);
        snd = 0;
        pthread_mutex_unlock(&lock);
    }

    return((void *)0);
}

int rtp()
{
    char buffer1[1000];
    char buffer2[1000];
    struct sockaddr_in pc_receive_addr;
    struct sockaddr_in pc_transmit_addr;
    int i = 0;
    pthread_t threadId;
    int recv_len;
    pthread_mutex_t lock;

    pthread_mutex_init(&lock, NULL);

    socklen_t len = sizeof(pc_receive_addr);
    if (rtp_init((struct sockaddr_in*)&pc_transmit_addr) != 0) {
        printf("initialize rtp error!\n");
        return -1;
    }

    /* set sip socket to non block mode */
    fcntl(config.sip_socket, F_SETFL, O_NONBLOCK); 

    int ring_fd = open(config.ring_name, O_RDONLY);
    if (ring_fd == -1) {
        printf("ring file doesn't exist!\n");
    }

    pthread_create(&threadId, NULL, send_serial, NULL);

    struct timeval old_time;
    struct timeval cur_time;
    gettimeofday(&old_time, 0);
    long usec = 0;   
    int j = 0;

    while (1) {
        /* ----- rtp receive ----- */
        if (i == 0) {

            for (j = 0; j < 10; j++) {
                recv_len = recvfrom(config.rtp_socket, buffer1 + j*100, 100, 0, (struct sockaddr*)&pc_receive_addr, &len);
                if (recv_len < 100)
                     printf("error!\n");
            }

/*        if (500 != read(ring_fd, buffer1, 500)) {
            lseek(fd, 0, SEEK_SET);
            read(fd, buffer1, 500);
        }*/

//         for (j = 0; j < 5; j++) {
//             usec = 0;
//             while (1) {
//                 gettimeofday(&cur_time, 0);
//                 /* calcuate the time difference in mircoseconds & store it to usec variable */
//                 if (cur_time.tv_sec - old_time.tv_sec > 0)
//                     usec = cur_time.tv_usec+1000000*(cur_time.tv_sec - old_time.tv_sec);
//                 else
//                     usec = cur_time.tv_usec;
// 
//                 /* if difference is larger than 50ms then transmit the data 
//                  * and if the difference is smaller than 40ms then sleep 10ms */
//                 if (usec - old_time.tv_usec >= 50000){
//                     old_time = cur_time;     
//                     break;
//                 }
//                 else if (usec - old_time.tv_usec <= 40000)
//                     usleep(10000);
//             }
//             serial_msg.data = buffer1 + j*100;
//             serial_txd(config.serial_voice, serial_msg);
//         }

        //          i = 1;
/*            while (snd == 1 ) {
                usleep(5000);
                continue;
            }
            pthread_mutex_lock(&lock);
            snd = 1;
            buf = buffer1;
            pthread_mutex_unlock(&lock);*/
//        } else if (i == 1) {
        /*recv_len = recvfrom(config.rtp_socket, buffer2, 5*serial_msg.data_length, 0, (struct sockaddr*)&pc_receive_addr, &len);
        if (recv_len < 5*serial_msg.data_length)
            printf("error!\n");*/

        /*if (500 != read(fd, buffer2, 500)) {
            lseek(fd, 0, SEEK_SET);
            read(fd, buffer2, 500);
        }
        i = 0; 

        while (snd == 1 ) {
            usleep(5000);
            continue;
        }
        pthread_mutex_lock(&lock);
        snd = 1;
        buf = buffer2;
        pthread_mutex_unlock(&lock);
    }*/

//        printf("recv_len is %d !\n", recv_len);

        /* ----- audio serial send ----- */
/*        if (0 != serial_txd(config.serial_voice, serial_msg))
            printf("transmit error!\n");*/

        /* ----- audio serial receive ----- */
        /*if (0 != serial_rxd(config.serial_voice, &serial_msg))
            printf("receive error!\n");*/

        if (0) {
            /* check message type */
            if (serial_msg.msg_type == (unsigned char)BYE) {
                /* receive bye message, then transmit bye and wait for sip socket OK message */
                serial_sip_cpy();
                sendzou(config.sip_socket, &sip_msg, SIP_SIZE, 0);
                if (sip_serial_txd(OK, TIMEOUT) != OK) /* if it does not receiv OK message */
                    serial_err();
                break;
            } else if (serial_msg.msg_type != 0xEE) {
                /* if it does not receive audio message, then ignore it */
                continue;
            }
        }
        /* check rssi value */
//        if (rssi_compare(serial_msg.rssi) == 1) {
        /* transmit transfer message */
//            transfer_notify();
//            break;
//        }

        /* ----- rtp send ----- */
/*        if (sendto(config.rtp_socket, serial_msg.data, serial_msg.data_length, 0, (struct sockaddr*)&pc_transmit_addr, len) 
            != serial_msg.data_length) {
            printf("rtp transmission number error!\n");
            continue;
        }*/

        /* ----- check if sip socket comes in new message */
/*        if (recvzou(config.sip_socket, (char *)&sip_msg, SIP_SIZE, 0) != -1) {
            if (sip_msg.msg_type == BYE) {
                sip_serial_cpy();
                serial_txd(config.serial_voice, serial_msg);

                if (serial_sip_txd((unsigned char)OK, TIMEOUT) != (unsigned char)OK)
                    sip_err();
                break;
            }
        }*/
    }

    rtp_close();

    return 0;
}

int main() {
    printf("serial size is %d !\n", SERIAL_SIZE);
    memset(&config, 0, sizeof(cfg_t));
    /* main configuration default value */
    config.serial_port_name[1] = (char *)malloc(30); /* control serial port name */
    config.serial_port_name[0] = (char *)malloc(30); /* audio serial port name */
    strcpy(config.serial_port_name[0], "/dev/ttyUSB0"); /* voice */
    strcpy(config.serial_port_name[1], "/dev/ttyS0"); /* control */
    config.ring_name = (char *)malloc(100);
    strcpy(config.ring_name, "ring");
    config.ap_ip = (char *)malloc(30);
    config.pc_ip = (char *)malloc(30);
    strcpy(config.ap_ip, "172.21.134.120");
    strcpy(config.pc_ip, "172.21.134.121");
    config.ap_id = 0x5678;
    config.sdr_id = 0x0000;
    config.ring_lock = FREE;
    config.ap_rtp_port = 10500;
    config.pc_rtp_port = 0;
    config.sip_port = 5060;
    config.rssi = 0x00;
    config.sip_socket = 0;
    config.rtp_socket = 0;

    unsigned char* serial_msg_temp;
//    serial_msg.data = (unsigned char*)malloc(160);

    if (serial_init() != 0) {
        printf("initialize serial error!\n");
        return -1;
    }

/*    if (sip_init() != 0) {
                printf("initialize sip error!\n");
                return -1;
        }*/
    config.sip_socket = open("sip_init_command", O_RDONLY);
    if (config.sip_socket <= 0) {
        printf("initialize sip error!\n");
        return -1;
    }

    if (0) {
        fd_set input;
        int max_fd;
        FD_ZERO(&input);
        FD_SET(config.sip_socket, &input);
        FD_SET(config.serial_control, &input);
        max_fd = (config.sip_socket > config.serial_control ? config.sip_socket : config.serial_control) + 1;

        while (1) {
            select(max_fd, &input, NULL, NULL, NULL);

            if (FD_ISSET(config.sip_socket, &input)) {
                if (sip_loop() == 0) /* the function runs sucessfully, then open rtp */
                    rtp();
            }

            if (FD_ISSET(config.serial_control, &input)) {
                if (serial_loop() == 0) /* the function runs sucessfully, then open rtp */
                    rtp();
            }
        }

        /* clean some configuration and message */
        config.pc_rtp_port = 0;
        config.ring_lock = FREE;
        config.rssi = 0;
        config.rtp_socket = 0;
        config.sdr_id = 0;

        serial_msg_temp = serial_msg.data;
        memset(&serial_msg_temp, 0, SERIAL_SIZE);
        serial_msg.data = serial_msg_temp;
        serial_msg_temp = NULL;
        memset(&sip_msg, 0, SIP_SIZE);
    }

//    if (sip_loop() == 0) /* the function runs sucessfully, then open rtp */
//        rtp();

    /* open thread to transmit ring audio */
/*    config.ring_lock = FREE;
    pthread_t threadId;
    pthread_create(&threadId, NULL, ring_txd, NULL);*/
    if (0) {
        void *t;
        ring_txd(t);
    }

    if (1) {
        serial_msg.ap_id = 0x5678;
        serial_msg.begin_syn = 0x55;
        serial_msg.channel = 0x02;
        serial_msg.CRC = 0x00;
        serial_msg.data_length = 100;
        serial_msg.end_syn = 0x55;
        serial_msg.msg_type = 0xee;
        serial_msg.rssi = 0x00;
        serial_msg.sdr_id = 0x01;
        config.pc_rtp_port = 10000;

        strcpy(config.pc_ip, "172.21.134.113");
        rtp();
    }

    usleep(100000000);

//    free(serial_msg.data);
    free(config.serial_port_name[0]);
    free(config.serial_port_name[1]);
    free(config.ring_name);
    free(config.ap_ip);
    free(config.pc_ip);

    return 0;
}




