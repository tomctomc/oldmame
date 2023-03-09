#ifndef YM3812INTF_H
#define YM3812INTF_H

#define MAX_3812 1

struct YM3812interface
{
	int num;
	int clock;
	int volume[MAX_3812];
};
#define YM3526interface YM3812interface


int YM3812_status_port_0_r(int offset);
int YM3812_read_port_0_r(int offset);
void YM3812_control_port_0_w(int offset,int data);
void YM3812_write_port_0_w(int offset,int data);
#define YM3526_status_port_0_r YM3812_status_port_0_r
#define YM3526_read_port_0_r YM3812_read_port_0_r
#define YM3526_control_port_0_w YM3812_control_port_0_w
#define YM3526_write_port_0_w YM3812_write_port_0_w

int YM3812_sh_start(struct YM3812interface *interface);
void YM3812_sh_stop(void);
void YM3812_sh_update(void);

#endif
