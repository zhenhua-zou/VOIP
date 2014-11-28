#include "struct.h"
#include "main.h"
#include "voip.h"

extern int flag = FREE;

int main()
{
    int i = 0;

    serial_mblk_t *mblk = malloc(sizeof(serial_mblk_t));
    mblk->type = 0x00;          /* regist */
    mblk->pc_id = 0xffff;
    mblk->ap_id = 0x5678;
    mblk->sdr_id = 0x1234;
    mblk->length = 0;
    mblk->data = NULL;

    /* serial pipe part */
    int fd[2];
    int serial_write_fd;
    int serial_read_fd;
    if (pipe(fd) < 0)
        return -1;
    serial_write_fd = fd[1];
    serial_read_fd = fd[0];

    /* voip pipe part */
    int voip_write_fd;
    int voip_read_fd;
    if (pipe(fd) < 0)
        return -1;
    voip_write_fd = fd[1];
    voip_read_fd = fd[0];

    /* if it is the control frame, we can do it like this */
    write(voip_write_fd, mblk, sizeof(serial_mblk_t)-5); /*  最后的指向数据的指针不用传递 */
    
    voip_instance(serial_write_fd, voip_read_fd, 0, i);

    free(mblk);
    return 0;
}
