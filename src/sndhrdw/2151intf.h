#ifndef YM2151INTF_H
#define YM2151INTF_H

#define MAX_2151 3

struct YM2151interface
{
	int num;
	int clock;
	int volume[MAX_2151];
	void (*handler[MAX_2151])(void);
};

int YM2151_status_port_0_r(int offset);
int YM2151_status_port_1_r(int offset);
int YM2151_status_port_2_r(int offset);

void YM2151_register_port_0_w(int offset,int data);
void YM2151_register_port_1_w(int offset,int data);
void YM2151_register_port_2_w(int offset,int data);

void YM2151_data_port_0_w(int offset,int data);
void YM2151_data_port_1_w(int offset,int data);
void YM2151_data_port_2_w(int offset,int data);
int YM2151_sh_start(struct YM2151interface *interface,int mode);
void YM2151_sh_stop(void);
void YM2151_sh_update(void);

#endif
