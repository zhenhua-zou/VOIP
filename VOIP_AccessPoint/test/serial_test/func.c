#include "../../src/main.h"

int timeout_flag = 0;

int serial_init()
{
    int fd[2];
    struct termios Opt;
    int speed = B115200;
    int i;

    for (i = 0; i < 2; i++)
    {
        /* open device */
        fd[i] = open(config.serial_port_name[i], O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd[i] == -1)
        {
            printf("open device %s error!\n", config.serial_port_name[i]);
            if (i == 1)      /* if serial 1 is open, then close it before exit */
                close(fd[0]);
            return -1;
        }

        /* set to normal(blocking) behavior */
        fcntl(fd[i], F_SETFL, 0);

        /* set input and output serial speed = 115200 */
        tcgetattr(fd[i], &Opt);
        tcflush(fd[i], TCIOFLUSH);
        cfsetispeed(&Opt, speed);
        cfsetospeed(&Opt, speed);
        if (tcsetattr(fd[i], TCSANOW, &Opt) != 0)
        {
            printf("set %s speed error!\n", config.serial_port_name[i]);
            if (i == 1)
                close(fd[1]);
            close(fd[0]);
            return -1;
        }
        tcflush(fd[i], TCIOFLUSH);

        /* set parity mode to 8N1 */
        tcgetattr(fd[i], &Opt);
        Opt.c_cflag &= ~PARENB;
        Opt.c_cflag &= ~CSTOPB;
        Opt.c_cflag &= ~CSIZE;
        Opt.c_cflag |= CS8;
        Opt.c_iflag &= ~INPCK;  /* don't check parity */
        Opt.c_cc[VTIME] = 0;    /* set time out time */
        Opt.c_cc[VMIN] = 0;
        if (tcsetattr(fd[i], TCSANOW, &Opt) != 0)
        {
            printf("set %s parity mode error!\n", config.serial_port_name[i]);
            if (i == 1)
                close(fd[1]);
            close(fd[0]);
            return -1;
        }
        tcflush(fd[i], TCIFLUSH);

        /* set raw input and output */
        tcgetattr(fd[i], &Opt);
        Opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        Opt.c_iflag |= IGNCR;
        Opt.c_oflag &= ~OPOST;
        if (tcsetattr(fd[i], TCSANOW, &Opt) != 0)
        {
            printf("set %s raw input and output error!\n", config.serial_port_name[i]);
            if (i == 1)
                close(fd[1]);
            close(fd[0]);
            return -1;
        }
        tcflush(fd[i], TCIOFLUSH);
    }

    /* done */
    config.serial_control = fd[0];
    config.serial_voice = fd[1];

    return 0;
}


int sip_init()
{
    struct sockaddr_in ap_addr;
    int sock;
    int addr_len;
    int len;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("socket");
        exit(errno);
    }
    else
        printf("create socket.\n");

    ap_addr.sin_family = AF_INET;
    ap_addr.sin_port = htons(AP_SIP_PORT);
    //ap_addr.sin_addr.s_addr = inet_addr(config.ap_ip);
    ap_addr.sin_addr.s_addr = INADDR_ANY;
 
    int opt = 1;    
    //setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, (socklen_t)setlen);
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    
    if ((bind(sock, (struct sockaddr *)&ap_addr, sizeof(ap_addr))) == -1)
    {
        perror("bind");
        exit(errno);
    }
    else
        printf("bind access to socket !\n\r");

    config.sip_socket = sock;
   
    config.pc_addr.sin_family = AF_INET;
    config.pc_addr.sin_port = htons(PC_SIP_PORT);
    config.pc_addr.sin_addr.s_addr = inet_addr(config.pc_ip);
    
    return 0;
}

void sip_serial_cpy()
{
    serial_msg.begin_syn = 0x55;
    serial_msg.sdr_id = sip_msg.sdr_id;
    serial_msg.ap_id = sip_msg.ap_id;
    serial_msg.msg_type = (unsigned char)sip_msg.msg_type;
    serial_msg.channel = 0x00;
    serial_msg.data_length = 0;
    serial_msg.rssi = 0x00;
    serial_msg.CRC = 0x00;
    serial_msg.end_syn = 0x55;
}

void serial_sip_cpy()
{
    sip_msg.msg_type = serial_msg.msg_type;
    sip_msg.sdr_id = serial_msg.sdr_id;
    sip_msg.ap_id = serial_msg.ap_id;
    sip_msg.ap_rtp_port = config.ap_rtp_port;
    sip_msg.pc_rtp_port = 0;
    sip_msg.rssi = serial_msg.rssi;
}

int serial_txd(int fd, SerialMSG_t msg)
{
    char buffer[200];
    int length;

    buffer[0] = msg.begin_syn;
    buffer[1] = msg.sdr_id;
    buffer[2] = (char)(msg.sdr_id/256);
    buffer[3] = msg.ap_id;
    buffer[4] = (char)(msg.ap_id/256);
    buffer[5] = msg.msg_type;
    //buffer[6] = msg.channel;
    buffer[6] = 0x04;
    buffer[7] = msg.data_length;

    length = msg.data_length;
    if (msg.data_length != 0)
    {
        memcpy(buffer+8, msg.data, msg.data_length);
    }
    
    buffer[8+length] = msg.rssi;
    buffer[9+length] = msg.CRC;
    buffer[10+length] = msg.end_syn;

    if (write(fd, buffer, 11+length) != 11+length)
        return -1;
//     if (write(fd, (char *)&msg, 8) != 8)
//         return -1;
// 
//     if (write(fd, (char *)&msg + 12, 3) != 3)
//         return -1;

    return 0;
}

int serial_rxd(int fd, SerialMSG_t* msg)
{
    struct termios Opt;
    tcgetattr(fd, &Opt);
    int data_length;

    char buf[20];

    if ((msg == NULL) || (msg->data == NULL))
    {
        printf("memory error!\n");
        return -1;
    }

    /* setting to nonblockmode, catching syn */
    fcntl(fd, F_SETFL, FNDELAY);

    if (read(fd, buf, 1) != 1)
        return -2;

    if (*buf != 0x55)
        return -1;

    /* setting to block mode */
    fcntl(fd, F_SETFL, 0);

    tcgetattr(fd, &Opt);
    Opt.c_cc[VMIN] = 7;
    Opt.c_cc[VTIME] = 1;
    if (tcsetattr(fd, TCSANOW, &Opt) != 0)
    {
        printf("set parity mode error!\n");
        return -1;
    }
    
    if (read(fd, buf+1, 7) != 7)
        return -1;

    data_length = *(buf+7);
    if (data_length != 0)
    {
        tcgetattr(fd, &Opt);
        Opt.c_cc[VMIN] = data_length;
        Opt.c_cc[VTIME] = 1;
        if (tcsetattr(fd, TCSANOW, &Opt) != 0)
        {
            printf("set parity mode error!\n");
            return -1;
        }

        if (read(fd, msg->data, data_length) != data_length)
            return -1;
    }

    tcgetattr(fd, &Opt);
    Opt.c_cc[VMIN] = 3;
    Opt.c_cc[VTIME] = 1;
    if (tcsetattr(fd, TCSANOW, &Opt) != 0)
    {
        printf("set parity mode error!\n");
        return -1;
    }

    if (read(fd, buf+8, 3) != 3)
        return -1;

    if (*(buf+10) != 0x55)
        return -1;

    /* check CRC !!!!!!!!!!!*/

    msg->begin_syn = *(buf);
    msg->sdr_id = *(buf+1) + 256*(*(buf+2));
    msg->ap_id = *(buf+3) + 256*(*(buf+4));
    msg->msg_type = *(buf+5);
    msg->channel = *(buf+6);
    msg->data_length = *(buf+7);
    msg->rssi = *(buf+8);
    msg->CRC = *(buf+9);
    msg->end_syn = *(buf+10);
    
    return 0;
}

void timeout_alarm(int i)
{
    timeout_flag = 1;
}

void set_timer(int i)
{
    struct itimerval itv, oldtv;
    itv.it_interval.tv_sec = i/1000;
    itv.it_interval.tv_usec = (i%1000)*1000;
    itv.it_value.tv_sec = i/1000;
    itv.it_value.tv_usec = (i%1000)*1000;

    setitimer(ITIMER_REAL, &itv, &oldtv);
}

unsigned char serial_sip_txd(unsigned char msg_type, int timeout)
{
    int i;
    signal(SIGALRM,timeout_alarm);
    set_timer(timeout);
    while (timeout_flag == 0)
    {
        i = serial_rxd(config.serial_control, &serial_msg);

        if (i == -2)
            usleep(10000);
        
        /* not receive anything or receive function error */
        if (i != 0)
            continue;

        /* if it is not register and transfer message, check of sdr_id & ap_id is necessary */
        if (((serial_msg.msg_type != (unsigned char)REGISTER)
              &&(serial_msg.msg_type != (unsigned char)TRANSFER))
                ||(config.sdr_id != 0x0000))
        {

            if (serial_msg.sdr_id != config.sdr_id)
                continue;

            /* the message is not for this ap */
            if (serial_msg.ap_id != config.ap_id)
                continue;
        }

        /* receive error message, then quit the rountine */
        if (serial_msg.msg_type == 0xFF)
        {
            set_timer(0);
            return -1;
        }

        /* check message type */
        if (msg_type != 0xFF)
        { /* if it has restriction*/
            if (serial_msg.msg_type != msg_type) /* if the message type is unmatch */
                continue;
        }

        /* yeah, the message is right. Then reset the timer and jump out of the loop */
        set_timer(0);
        break;
    }

    /* timeout occurs */
    if (timeout_flag == 1)
    {
        set_timer(0);
        timeout_flag = 0; /* reset the flag */
        return -1;
    }

    /* then move the message to sip port */
    serial_sip_cpy();

    socklen_t len = sizeof(struct sockaddr_in);
    /* if the received message is not specified, the trasnmission is carried out outside this rountine */
    if (msg_type != 0xFF)
        //if (send(config.sip_socket, &sip_msg, SIP_SIZE, 0) != SIP_SIZE)
        if (sendto(config.sip_socket, &sip_msg, SIP_SIZE, 0, (struct sockaddr*)&config.pc_addr, len) != SIP_SIZE)
            return -1;

    return serial_msg.msg_type;
}

SipMSG_Type_t sip_serial_txd(SipMSG_Type_t msg_type, int timeout)
{
    int i;
    signal(SIGALRM, timeout_alarm);
    set_timer(timeout);
    //fcntl(config.sip_socket, F_SETFL, O_NONBLOCK);
    fcntl(config.sip_socket, F_SETFL, 0);

    struct sockaddr_in recvfrom_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    while (timeout_flag == 0)
    {
        i = recvfrom(config.sip_socket, (char *)&sip_msg, SIP_SIZE, 0, (struct sockaddr*)&recvfrom_addr, &len);

        
        /* the message is not from the pc */
//         if (0 != memcmp(&recvfrom_addr, &config.pc_addr, sizeof(config.pc_addr)))
//             // zouzhenhuazouzhenhua
//             continue;

        /* did not receive anything */
        if (i == -1)
        {
//             perror("error is");
            continue;
        }

        /* receive length error */
        if (i != SIP_SIZE)
            continue;

        /* check message type */
        if (msg_type != ERROR)
        { /* if it has restriction*/
            if (sip_msg.msg_type != msg_type) /* if the message type is unmatch */
                continue;
        }

        /* if it is not invite message, check of sdr_id is necessary */
        if ((sip_msg.msg_type != INVITE)||(config.sdr_id != 0x0000))
        {
            if (sip_msg.sdr_id != config.sdr_id)
                continue;

            /* the message is not for this ap */
            /* if (sip_msg.ap_id != config.ap_id)
                        continue;
            */
        }

        /* receive error message, then quit the rountine */
        if (sip_msg.msg_type == ERROR)
        {
            set_timer(0);
            return -1;
        }

        /* yeah, the message is right. Then reset the timer and jump out of the loop */
        set_timer(0);
        break;
    }

    if (timeout_flag == 1)
    {
        set_timer(0);
        timeout_flag = 0; /* reset the flag */
        return -1;
    }

    /* then move the message to serial port */
    sip_serial_cpy();

    if (msg_type != ERROR)
        serial_txd(config.serial_control, serial_msg);

    /* if the received message is not specified, the trasnmission is carried out outside this rountine */
    return sip_msg.msg_type;
}

int transfer_notify()
{
    serial_msg.begin_syn = 0x55;
    serial_msg.msg_type = 0x02;     /* transfer message type */
    serial_msg.sdr_id = config.sdr_id;
    serial_msg.ap_id = config.ap_id;
    serial_msg.channel = 0x00;
    serial_msg.data_length = 0;
    serial_msg.rssi = 0x00;
    serial_msg.CRC = 0x00;
    serial_msg.end_syn = 0x55;

    return(serial_txd(config.serial_control, serial_msg));
}

void* ring_txd(void* arg)
{
    int fd;
    char buffer[VOICE_LENGTH + 11];
    int size = VOICE_LENGTH + 11;
    int times = 0;
    char *data = &buffer[8];

    fd = open(config.ring_name, O_RDONLY);
    if (fd == -1)
    {
        printf("ring file doesn't exist!\n");
    }

    /* build the message */
    buffer[0] = 0x55;
    buffer[1] = config.sdr_id;
    buffer[2] = (char)(config.sdr_id/256);
    buffer[3] = config.ap_id;
    buffer[4] = (char)(config.ap_id/256);
    buffer[5] = (unsigned char)RING; // RING type message
    buffer[6] = 0x04;
    buffer[7] = VOICE_LENGTH;
    buffer[VOICE_LENGTH + 8] = 0x00;
    buffer[VOICE_LENGTH + 9] = 0x00;
    buffer[VOICE_LENGTH + 10] = 0x55;
    
    struct timeval old_time;
    struct timeval cur_time;
    gettimeofday(&old_time, 0);
    int j = 0;

    long usec = 0;
    while (config.ring_lock == FREE)
    {
        if (VOICE_LENGTH != read(fd, data, VOICE_LENGTH))
        {
            lseek(fd, 0, SEEK_SET);
            read(fd, data, VOICE_LENGTH);
        }

        usec = 0;
        while (1)
        {
            gettimeofday(&cur_time, 0);
            /* calcuate the time difference in mircoseconds & store it to usec variable */
            if (cur_time.tv_sec - old_time.tv_sec > 0)
                usec = cur_time.tv_usec+1000000*(cur_time.tv_sec - old_time.tv_sec);
            else
                usec = cur_time.tv_usec;

            /* if difference is larger than 50ms then transmit the data
             * and if the difference is smaller than 40ms then sleep 10ms */
            if (usec - old_time.tv_usec >= 60000)
            {
                old_time = cur_time;
                break;
            }
            else if (usec - old_time.tv_usec <= 50000)
                usleep(10000);
        }
        
        //    serial_txd(config.serial_voice, ring_serial_msg);
        
        if ((VOICE_LENGTH+11) != write(config.serial_voice, buffer, (VOICE_LENGTH+11)))
            printf("write error!\n");
        
        times++;
    }

    config.ring_lock = USED;
    printf("ringing stopped !\n");
    close(fd);

    printf("times is %d!\n", times);

    return((void *)0);
}

int rssi_compare(unsigned char rssi)
{
    if (rssi < 0x5e)
        return 1;

    return 0;
}

int rtp_init(struct sockaddr_in* pc_addr)
{
    struct sockaddr_in ap_addr;
    int sock;
    int addr_len;
    int len;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("socket");
        exit(errno);
    }
    else
        printf("create socket.\n\r");

    ap_addr.sin_family = AF_INET;
    ap_addr.sin_port = htons(config.ap_rtp_port);
    ap_addr.sin_addr.s_addr = inet_addr(config.ap_ip);

    if ((bind(sock, (struct sockaddr *)&ap_addr, sizeof(ap_addr))) == -1 )
    {
        perror("bind");
        exit(errno);
    }
    else
        printf("bind access to socket !\n\r");

    config.rtp_socket = sock;

    pc_addr->sin_family = AF_INET;
    pc_addr->sin_port = htons(config.pc_rtp_port);
    pc_addr->sin_addr.s_addr = inet_addr(config.pc_ip);

    return 0;
}

int rtp_close()
{
    close(config.rtp_socket);
    return 0;
}
