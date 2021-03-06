/******************************************************************************

  Copyright (C), 2010-2012, Silead, Inc.

 ******************************************************************************
Filename      : gsl1680-d0.c
Version       : R2.0
Aurthor       : mark_huang
Creattime     : 2012.6.20
Description   : Driver for Silead I2C touchscreen.

 ******************************************************************************/

#include "tpd.h"
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/time.h>

#ifdef MT6573
#include <mach/mt6573_boot.h>
#endif
#ifdef MT6575
#include <mach/mt6575_boot.h>
#include <mach/mt6575_pm_ldo.h>
#include <mach/mt6575_typedefs.h>
#endif
#ifdef MT6577
#include <mach/mt6577_boot.h>
#include <mach/mt6577_pm_ldo.h>
#include <mach/mt6577_typedefs.h>
#endif
#ifdef MT6589
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#endif
#ifdef MT8389
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#endif
#ifdef MT6572
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#endif

#include "gsl_ts_driver.h"
#include <mach/mt_boot_common.h>
#include <mach/mt_pm_ldo.h>
#include <pmic_drv.h>
#include <cust_i2c.h>

#if defined(HCT_TP_GESTRUE)
#define  GSL_GESTURE
#endif
#ifdef CONFIG_GSL_TP_HALL_MODE
// 0:HALL_CLOSE, 1:HALL_OPEN	
static u8 gsl_hall_flag = 1; 
//extern static u8 kpd_hall_state;//皮套状态标志0表示合上，1表示打开
static u8 gsl_hall_data[8]={0};
#endif
extern struct tpd_device *tpd;
extern int gsl_obtain_gesture();
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En, kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),kal_bool auto_umask);

static struct gsl_ts_data *ddata = NULL;

static int boot_mode = NORMAL_BOOT;

#define GSL_DEV_NAME "gsl1680"

#define I2C_TRANS_SPEED 400	//100 khz or 400 khz
#define TPD_REG_BASE 0x00


static struct tpd_bootloader_data_t g_bootloader_data;
static const struct i2c_device_id gsl_device_id[] = {{"gsl_tp",0},{}};
static unsigned short force[] = {0,0x80,I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
static struct i2c_board_info __initdata i2c_tpd = { I2C_BOARD_INFO("gsl_tp", (0x80>>1))};

static volatile int gsl_halt_flag = 0;
static struct mutex gsl_i2c_lock;

#ifdef GSL_GESTURE
static char tpgesture_value[10]={};
typedef enum{
	GE_DISABLE = 0,
	GE_ENABLE = 1,
	GE_WAKEUP = 2,
	GE_NOWORK =3,
}GE_T;
static GE_T gsl_gesture_status = GE_DISABLE;
static volatile unsigned int gsl_gesture_flag = 1;
static char gsl_gesture_c = 0;
extern int gsl_obtain_gesture(void);
extern void gsl_FunIICRead(unsigned int (*fun) (unsigned int *,unsigned int,unsigned int));
extern void tpgesture_hander();
#endif

#ifdef TPD_PROXIMITY
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/wakelock.h>
static u8 tpd_proximity_flag = 0; //flag whether start alps
static u8 tpd_proximity_detect = 1;//0-->close ; 1--> far away
static struct wake_lock ps_lock;
static u8 gsl_psensor_data[8]={0};
#endif

#ifdef GSL_TIMER
#define GSL_TIMER_CHECK_CIRCLE        200
static struct delayed_work gsl_timer_check_work;
static struct workqueue_struct *gsl_timer_workqueue = NULL;
static char int_1st[4];
static char int_2nd[4];
#endif

#ifdef TPD_PROC_DEBUG
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/seq_file.h>
static struct proc_dir_entry *gsl_config_proc = NULL;
#define GSL_CONFIG_PROC_FILE "gsl_config"
#define CONFIG_LEN 31
static char gsl_read[CONFIG_LEN];
static u8 gsl_data_proc[8] = {0};
static u8 gsl_proc_flag = 0;
#endif

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static struct task_struct *thread = NULL;
static int tpd_flag = 0;

#ifdef GSL_DEBUG 
#define print_info(fmt, args...)   \
		do{                              \
		    printk("[tp-gsl][%s]"fmt,__func__, ##args);     \
		}while(0)
#else
#define print_info(fmt, args...)
#endif

#ifdef TPD_HAVE_BUTTON
extern void tpd_button(unsigned int x, unsigned int y, unsigned int down);
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

static int gsl_read_interface(struct i2c_client *client,
        u8 reg, u8 *buf, u32 num)
{
	int err = 0;
	int i;
	u8 temp = reg;
	mutex_lock(&gsl_i2c_lock);
	if(temp < 0x80)
	{
		temp = (temp+8)&0x5c;
			i2c_master_send(client,&temp,1);	
			err = i2c_master_recv(client,&buf[0],4);
		temp = reg;
		i2c_master_send(client,&temp,1);
		err = i2c_master_recv(client,&buf[0],4);
	}
	for(i=0;i<num;)
	{	
		temp = reg + i;
		i2c_master_send(client,&temp,1);
		if((i+8)<num)
			err = i2c_master_recv(client,(buf+i),8);
		else
			err = i2c_master_recv(client,(buf+i),(num-i));
		i+=8;
	}
	mutex_unlock(&gsl_i2c_lock);

	return err;
}

static int gsl_write_interface(struct i2c_client *client,
        const u8 reg, u8 *buf, u32 num)
{
	struct i2c_msg xfer_msg[1] = {0};
	int err;
	u8 tmp_buf[num + 1];

	tmp_buf[0] = reg;
	memcpy(tmp_buf + 1, buf, num);

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = num + 1;
	xfer_msg[0].flags = 0;//client->flags & I2C_M_TEN;
	xfer_msg[0].buf = tmp_buf;
	xfer_msg[0].timing = 400;//I2C_TRANS_SPEED;
	mutex_lock(&gsl_i2c_lock);

	err = i2c_transfer(client->adapter, xfer_msg, 1);
	mutex_unlock(&gsl_i2c_lock);

	
	return err;
}
#ifdef GSL_GESTURE
static unsigned int gsl_read_oneframe_data(unsigned int *data,
				unsigned int addr,unsigned int len)
{
	u8 buf[4];
	int i;
	printk("tp-gsl-gesture %s\n",__func__);
	printk("gsl_read_oneframe_data:::addr=%x,len=%x\n",addr,len);
	for(i=0;i<len/2;i++){
		buf[0] = ((addr+i*8)/0x80)&0xff;
		buf[1] = (((addr+i*8)/0x80)>>8)&0xff;
		buf[2] = (((addr+i*8)/0x80)>>16)&0xff;
		buf[3] = (((addr+i*8)/0x80)>>24)&0xff;
		gsl_write_interface(ddata->client,0xf0,buf,4);
		gsl_read_interface(ddata->client,(addr+i*8)%0x80,(char *)&data[i*2],8);
	}
	if(len%2){
		buf[0] = ((addr+len*4 - 4)/0x80)&0xff;
		buf[1] = (((addr+len*4 - 4)/0x80)>>8)&0xff;
		buf[2] = (((addr+len*4 - 4)/0x80)>>16)&0xff;
		buf[3] = (((addr+len*4 - 4)/0x80)>>24)&0xff;
		gsl_write_interface(ddata->client,0xf0,buf,4);
		gsl_read_interface(ddata->client,(addr+len*4 - 4)%0x80,(char *)&data[len-1],4);
	}
	#if 1
	for(i=0;i<len;i++){
	printk("gsl_read_oneframe_data =%x\n",data[i]);	
	//printk("gsl_read_oneframe_data =%x\n",data[len-1]);
	}
	#endif
	
	return len;
}
#endif

static int gsl_test_i2c(struct i2c_client *client)
{
	int i,err;
	u8 buf[4]={0};
	for(i=0;i<5;i++)
	{
		err=gsl_read_interface(client,0xfc,buf,4);
		if(err>0)
		{
			printk("[tp-gsl] i2c read 0xfc = 0x%02x%02x%02x%02x\n",
				buf[3],buf[2],buf[1],buf[0]);
			break;
		}
	}
	return (err<0?-1:0);
}


static void gsl_io_control(struct i2c_client *client)
{
	u8 buf[4] = {0};
	int i;
#if GSL9XX_VDDIO_1800
	for(i=0;i<5;i++){
		buf[0] = 0;
		buf[1] = 0;
		buf[2] = 0xfe;
		buf[3] = 0x1;
		gsl_write_interface(client,0xf0,buf,4);
		buf[0] = 0x5;
		buf[1] = 0;
		buf[2] = 0;
		buf[3] = 0x80;
		gsl_write_interface(client,0x78,buf,4);
		msleep(5);
	}
	msleep(50);
#endif
}
static void gsl_start_core(struct i2c_client *client)
{
	u8 buf[4] = {0};
	buf[0]=0;
	gsl_write_interface(client,0xe0,buf,4);
#ifdef GSL_ALG_ID
	gsl_DataInit(gsl_cfg_table[gsl_cfg_index].data_id);
#endif
}

static void gsl_reset_core(struct i2c_client *client)
{
	u8 buf[4] = {0x00};
	
	buf[0] = 0x88;
	gsl_write_interface(client,0xe0,buf,4);
	msleep(5);

	buf[0] = 0x04;
	gsl_write_interface(client,0xe4,buf,4);
	msleep(5);
	
	buf[0] = 0;
	gsl_write_interface(client,0xbc,buf,4);
	msleep(5);
	gsl_io_control(client);
}

static void gsl_clear_reg(struct i2c_client *client)
{
	u8 buf[4]={0};
	//clear reg
	buf[0]=0x88;
	gsl_write_interface(client,0xe0,buf,4);
	msleep(20);
	buf[0]=0x3;
	gsl_write_interface(client,0x80,buf,4);
	msleep(5);
	buf[0]=0x4;
	gsl_write_interface(client,0xe4,buf,4);
	msleep(5);
	buf[0]=0x0;
	gsl_write_interface(client,0xe0,buf,4);
	msleep(20);
	//clear reg
}

#if 0
#define DMA_TRANS_LEN 0x20
static void gsl_load_fw(struct i2c_client *client,const struct fw_data *GSL_DOWNLOAD_DATA,int data_len)
{
	u8 buf[DMA_TRANS_LEN*4] = {0};
	u8 send_flag = 1;
	u8 addr=0;
	u32 source_line = 0;
	u32 source_len = data_len;//ARRAY_SIZE(GSL_DOWNLOAD_DATA);

	print_info("=============gsl_load_fw start==============\n");

	for (source_line = 0; source_line < source_len; source_line++) 
	{
		/* init page trans, set the page val */
		if (0xf0 == GSL_DOWNLOAD_DATA[source_line].offset)
		{
			memcpy(buf,&GSL_DOWNLOAD_DATA[source_line].val,4);	
			gsl_write_interface(client, 0xf0, buf, 4);
			send_flag = 1;
		}
		else 
		{
			if (1 == send_flag % (DMA_TRANS_LEN < 0x20 ? DMA_TRANS_LEN : 0x20))
	    			addr = (u8)GSL_DOWNLOAD_DATA[source_line].offset;

			memcpy((buf+send_flag*4 -4),&GSL_DOWNLOAD_DATA[source_line].val,4);	

			if (0 == send_flag % (DMA_TRANS_LEN < 0x20 ? DMA_TRANS_LEN : 0x20)) 
			{
	    		gsl_write_interface(client, addr, buf, DMA_TRANS_LEN * 4);
				send_flag = 0;
			}

			send_flag++;
		}
	}

	print_info("=============gsl_load_fw end==============\n");

}
#else 
static void gsl_load_fw(struct i2c_client *client,const struct fw_data *GSL_DOWNLOAD_DATA,int data_len)
{
	u8 buf[4] = {0};
	//u8 send_flag = 1;
	u8 addr=0;
	u32 source_line = 0;
	u32 source_len = data_len;//ARRAY_SIZE(GSL_DOWNLOAD_DATA);

	print_info("=============gsl_load_fw start==============\n");

	for (source_line = 0; source_line < source_len; source_line++) 
	{
		/* init page trans, set the page val */
    	addr = (u8)GSL_DOWNLOAD_DATA[source_line].offset;
		memcpy(buf,&GSL_DOWNLOAD_DATA[source_line].val,4);
    	gsl_write_interface(client, addr, buf, 4);	
	}
}
#endif

static void gsl_sw_init(struct i2c_client *client)
{
	int temp;
	static volatile int gsl_sw_flag=0;
	if(1==gsl_sw_flag)
		return;
	gsl_sw_flag=1;
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(20);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(20);
	
	temp = gsl_test_i2c(client);
	if(temp<0){
		gsl_sw_flag = 0;
		return;
	}

	gsl_clear_reg(client);
	gsl_reset_core(client);

	gsl_io_control(client);
	gsl_load_fw(client,gsl_cfg_table[gsl_cfg_index].fw,
		gsl_cfg_table[gsl_cfg_index].fw_size);
	gsl_io_control(client);

	gsl_start_core(client);
	gsl_sw_flag=0;
}

static void check_mem_data(struct i2c_client *client)
{
	char read_buf[4] = {0};
	gsl_read_interface(client, 0xb0, read_buf, 4);

	print_info("[gsl1680][%s] addr = 0xb0; read_buf = %02x%02x%02x%02x\n",
		__func__, read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
	if (read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a)
	{
		gsl_sw_init(client);
	}
}

#ifdef TPD_PROC_DEBUG
#define GSL_APPLICATION
#ifdef GSL_APPLICATION
static int gsl_read_MorePage(struct i2c_client *client,u32 addr,u8 *buf,u32 num)
{
	int i;
	u8 tmp_buf[4] = {0};
	u8 tmp_addr;
	for(i=0;i<num/8;i++){
		tmp_buf[0]=(char)((addr+i*8)/0x80);
		tmp_buf[1]=(char)(((addr+i*8)/0x80)>>8);
		tmp_buf[2]=(char)(((addr+i*8)/0x80)>>16);
		tmp_buf[3]=(char)(((addr+i*8)/0x80)>>24);
		gsl_write_interface(client,0xf0,tmp_buf,4);
		tmp_addr = (char)((addr+i*8)%0x80);
		gsl_read_interface(client,tmp_addr,(buf+i*8),8);
	}
	if(i*8<num){
		tmp_buf[0]=(char)((addr+i*8)/0x80);
		tmp_buf[1]=(char)(((addr+i*8)/0x80)>>8);
		tmp_buf[2]=(char)(((addr+i*8)/0x80)>>16);
		tmp_buf[3]=(char)(((addr+i*8)/0x80)>>24);
		gsl_write_interface(client,0xf0,tmp_buf,4);
		tmp_addr = (char)((addr+i*8)%0x80);
		gsl_read_interface(client,tmp_addr,(buf+i*8),4);
	}
}
#endif
static int char_to_int(char ch)
{
	if(ch>='0' && ch<='9')
		return (ch-'0');
	else
		return (ch-'a'+10);
}

//static int gsl_config_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
static int gsl_config_read_proc(struct seq_file *m,void *v)
{
	char temp_data[5] = {0};
	//int i;
	unsigned int tmp=0;
	if('v'==gsl_read[0]&&'s'==gsl_read[1])
	{
#ifdef GSL_ALG_ID
		tmp=gsl_version_id();
#else 
		tmp=0x20121215;
#endif
		seq_printf(m,"version:%x\n",tmp);
	}
	else if('r'==gsl_read[0]&&'e'==gsl_read[1])
	{
		if('i'==gsl_read[3])
		{
#ifdef GSL_ALG_ID 
			tmp=(gsl_data_proc[5]<<8) | gsl_data_proc[4];
			seq_printf(m,"gsl_config_data_id[%d] = ",tmp);
			if(tmp>=0&&tmp<gsl_cfg_table[gsl_cfg_index].data_size)
				seq_printf(m,"%d\n",gsl_cfg_table[gsl_cfg_index].data_id[tmp]); 
#endif
		}
		else 
		{
			gsl_write_interface(ddata->client,0xf0,&gsl_data_proc[4],4);
			gsl_read_interface(ddata->client,gsl_data_proc[0],temp_data,4);
			seq_printf(m,"offset : {0x%02x,0x",gsl_data_proc[0]);
			seq_printf(m,"%02x",temp_data[3]);
			seq_printf(m,"%02x",temp_data[2]);
			seq_printf(m,"%02x",temp_data[1]);
			seq_printf(m,"%02x};\n",temp_data[0]);
		}
	}
#ifdef GSL_APPLICATION
	else if('a'==gsl_read[0]&&'p'==gsl_read[1]){
		char *buf;
		int temp1;
		tmp = (unsigned int)(((gsl_data_proc[2]<<8)|gsl_data_proc[1])&0xffff);
		buf=kzalloc(tmp,GFP_KERNEL);
		if(buf==NULL)
			return -1;
		if(3==gsl_data_proc[0]){
			gsl_read_interface(ddata->client,gsl_data_proc[3],buf,tmp);
			if(tmp < m->size){
				memcpy(m->buf,buf,tmp);
			}
		}else if(4==gsl_data_proc[0]){
			temp1=((gsl_data_proc[6]<<24)|(gsl_data_proc[5]<<16)|
				(gsl_data_proc[4]<<8)|gsl_data_proc[3]);
			gsl_read_MorePage(ddata->client,temp1,buf,tmp);
			if(tmp < m->size){
				memcpy(m->buf,buf,tmp);
			}
		}
		kfree(buf);
	}
#endif
	return 0;
}
static int gsl_config_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
	u8 buf[8] = {0};
	char temp_buf[CONFIG_LEN];
	char *path_buf;
	int tmp = 0;
	int tmp1 = 0;
	print_info("[tp-gsl][%s] \n",__func__);
	if(count > 512)
	{
		print_info("size not match [%d:%ld]\n", CONFIG_LEN, count);
        	return -EFAULT;
	}
	path_buf=kzalloc(count,GFP_KERNEL);
	if(!path_buf)
	{
		printk("alloc path_buf memory error \n");
		return -1;
	}	
	if(copy_from_user(path_buf, buffer, count))
	{
		print_info("copy from user fail\n");
		goto exit_write_proc_out;
	}
	memcpy(temp_buf,path_buf,(count<CONFIG_LEN?count:CONFIG_LEN));
	print_info("[tp-gsl][%s][%s]\n",__func__,temp_buf);
#ifdef GSL_APPLICATION
	if('a'!=temp_buf[0]||'p'!=temp_buf[1]){
#endif
	buf[3]=char_to_int(temp_buf[14])<<4 | char_to_int(temp_buf[15]);	
	buf[2]=char_to_int(temp_buf[16])<<4 | char_to_int(temp_buf[17]);
	buf[1]=char_to_int(temp_buf[18])<<4 | char_to_int(temp_buf[19]);
	buf[0]=char_to_int(temp_buf[20])<<4 | char_to_int(temp_buf[21]);
	
	buf[7]=char_to_int(temp_buf[5])<<4 | char_to_int(temp_buf[6]);
	buf[6]=char_to_int(temp_buf[7])<<4 | char_to_int(temp_buf[8]);
	buf[5]=char_to_int(temp_buf[9])<<4 | char_to_int(temp_buf[10]);
	buf[4]=char_to_int(temp_buf[11])<<4 | char_to_int(temp_buf[12]);
#ifdef GSL_APPLICATION
	}
#endif
	if('v'==temp_buf[0]&& 's'==temp_buf[1])//version //vs
	{
		memcpy(gsl_read,temp_buf,4);
		printk("gsl version\n");
	}
	else if('s'==temp_buf[0]&& 't'==temp_buf[1])//start //st
	{
	#ifdef GSL_TIMER	
		cancel_delayed_work_sync(&gsl_timer_check_work);
	#endif
		gsl_proc_flag = 1;
		gsl_reset_core(ddata->client);
	}
	else if('e'==temp_buf[0]&&'n'==temp_buf[1])//end //en
	{
		msleep(20);
		gsl_reset_core(ddata->client);
		gsl_start_core(ddata->client);
		gsl_proc_flag = 0;
	}
	else if('r'==temp_buf[0]&&'e'==temp_buf[1])//read buf //
	{
		memcpy(gsl_read,temp_buf,4);
		memcpy(gsl_data_proc,buf,8);
	}
	else if('w'==temp_buf[0]&&'r'==temp_buf[1])//write buf
	{
		gsl_write_interface(ddata->client,buf[4],buf,4);
	}
	
#ifdef GSL_ALG_ID
	else if('i'==temp_buf[0]&&'d'==temp_buf[1])//write id config //
	{
		tmp1=(buf[7]<<24)|(buf[6]<<16)|(buf[5]<<8)|buf[4];
		tmp=(buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
		if(tmp1>=0 && tmp1<gsl_cfg_table[gsl_cfg_index].data_size)
		{
			gsl_cfg_table[gsl_cfg_index].data_id[tmp1] = tmp;
		}
	}
#endif
#ifdef GSL_APPLICATION
	else if('a'==temp_buf[0]&&'p'==temp_buf[1]){
		if(1==path_buf[3]){
			tmp=((path_buf[5]<<8)|path_buf[4]);
			gsl_write_interface(ddata->client,path_buf[6],&path_buf[10],tmp);
		}else if(2==path_buf[3]){
			tmp = ((path_buf[5]<<8)|path_buf[4]);
			tmp1=((path_buf[9]<<24)|(path_buf[8]<<16)|(path_buf[7]<<8)
				|path_buf[6]);
			buf[0]=(char)((tmp1/0x80)&0xff);
			buf[1]=(char)(((tmp1/0x80)>>8)&0xff);
			buf[2]=(char)(((tmp1/0x80)>>16)&0xff);
			buf[3]=(char)(((tmp1/0x80)>>24)&0xff);
			buf[4]=(char)(tmp1%0x80);
			gsl_write_interface(ddata->client,0xf0,buf,4);
			gsl_write_interface(ddata->client,buf[4],&path_buf[10],tmp);
		}else if(3==path_buf[3]||4==path_buf[3]){
			memcpy(gsl_read,temp_buf,4);
			memcpy(gsl_data_proc,&path_buf[3],7);
		}
	}
#endif
exit_write_proc_out:
	kfree(path_buf);
	return count;
}
static int gsl_server_list_open(struct inode *inode,struct file *file)
{
	return single_open(file,gsl_config_read_proc,NULL);
}
static const struct file_operations gsl_seq_fops = {
	.open = gsl_server_list_open,
	.read = seq_read,
	.release = single_release,
	.write = gsl_config_write_proc,
	.owner = THIS_MODULE,
};
#endif

#ifdef GSL_TIMER
static void gsl_timer_check_func(struct work_struct *work)
{
	struct gsl_ts_data *ts = ddata;
	struct i2c_client *gsl_client = ts->client;
	static int i2c_lock_flag = 0;
	char read_buf[4]  = {0};
	char init_chip_flag = 0;
	int i,flag;
	print_info("----------------gsl_monitor_worker-----------------\n");	

	if(i2c_lock_flag != 0)
		return;
	else
		i2c_lock_flag = 1;

	gsl_read_interface(gsl_client, 0xb4, read_buf, 4);
	memcpy(int_2nd,int_1st,4);
	memcpy(int_1st,read_buf,4);

	if(int_1st[3] == int_2nd[3] && int_1st[2] == int_2nd[2] &&
		int_1st[1] == int_2nd[1] && int_1st[0] == int_2nd[0])
	{
		printk("======int_1st: %x %x %x %x , int_2nd: %x %x %x %x ======\n",
			int_1st[3], int_1st[2], int_1st[1], int_1st[0], 
			int_2nd[3], int_2nd[2],int_2nd[1],int_2nd[0]);
		init_chip_flag = 1;
		goto queue_monitor_work;
	}
	/*check 0xb0 register,check firmware if ok*/
	for(i=0;i<5;i++){
		gsl_read_interface(gsl_client, 0xb0, read_buf, 4);
		if(read_buf[3] != 0x5a || read_buf[2] != 0x5a || 
			read_buf[1] != 0x5a || read_buf[0] != 0x5a){
			printk("gsl_monitor_worker 0xb0 = {0x%02x%02x%02x%02x};\n",
				read_buf[3],read_buf[2],read_buf[1],read_buf[0]);
			flag = 1;
		}else{
			flag = 0;
			break;
		}

	}
	if(flag == 1){
		init_chip_flag = 1;
		goto queue_monitor_work;
	}
	
	/*check 0xbc register,check dac if normal*/
	for(i=0;i<5;i++){
		gsl_read_interface(gsl_client, 0xbc, read_buf, 4);
		if(read_buf[3] != 0 || read_buf[2] != 0 || 
			read_buf[1] != 0 || read_buf[0] != 0){
			flag = 1;
		}else{
			flag = 0;
			break;
		}
	}
	if(flag == 1){
		gsl_reset_core(gsl_client);
		gsl_start_core(gsl_client);
		init_chip_flag = 0;
	}
queue_monitor_work:
	if(init_chip_flag){
		gsl_sw_init(gsl_client);
		memset(int_1st,0xff,sizeof(int_1st));
	}
	
	if(gsl_halt_flag==0){
		queue_delayed_work(gsl_timer_workqueue, &gsl_timer_check_work, 200);
	}
	i2c_lock_flag = 0;

}
#endif

#ifdef TPD_PROXIMITY
static void gsl_gain_psensor_data(struct i2c_client *client)
{
	u8 buf[4]={0};
	/**************************/
	buf[0]=0x3;
	gsl_write_interface(client,0xf0,buf,4);
	gsl_read_interface(client,0,&gsl_psensor_data[0],4);
	/**************************/
	
	buf[0]=0x4;
	gsl_write_interface(client,0xf0,buf,4);
	gsl_read_interface(client,0,&gsl_psensor_data[4],4);
	/**************************/
}

static int tpd_get_ps_value(void)
{
	return tpd_proximity_detect;
}
static int tpd_enable_ps(int enable)
{
	u8 buf[4];
	if (enable) {
		wake_lock(&ps_lock);
		buf[3] = 0x00;
		buf[2] = 0x00;
		buf[1] = 0x00;
		buf[0] = 0x3;
		gsl_write_interface(ddata->client, 0xf0, buf, 4);
		buf[3] = 0x5a;
		buf[2] = 0x5a;
		buf[1] = 0x5a;
		buf[0] = 0x5a;
		gsl_write_interface(ddata->client, 0, buf, 4);

		buf[3] = 0x00;
		buf[2] = 0x00;
		buf[1] = 0x00;
		buf[0] = 0x4;
		gsl_write_interface(ddata->client, 0xf0, buf, 4);
		buf[3] = 0x0;
		buf[2] = 0x0;
		buf[1] = 0x0;
		buf[0] = 0x2;
		gsl_write_interface(ddata->client, 0, buf, 4);
		
		tpd_proximity_flag = 1;
		//add alps of function
		printk("tpd-ps function is on\n");
	} else {
		tpd_proximity_flag = 0;
		wake_unlock(&ps_lock);
		buf[3] = 0x00;
		buf[2] = 0x00;
		buf[1] = 0x00;
		buf[0] = 0x3;
		gsl_write_interface(ddata->client, 0xf0, buf, 4);
		buf[3] = gsl_psensor_data[3];
		buf[2] = gsl_psensor_data[2];
		buf[1] = gsl_psensor_data[1];
		buf[0] = gsl_psensor_data[0];
		gsl_write_interface(ddata->client, 0, buf, 4);

		buf[3] = 0x00;
		buf[2] = 0x00;
		buf[1] = 0x00;
		buf[0] = 0x4;
		gsl_write_interface(ddata->client, 0xf0, buf, 4);
		buf[3] = gsl_psensor_data[7];
		buf[2] = gsl_psensor_data[6];
		buf[1] = gsl_psensor_data[5];
		buf[0] = gsl_psensor_data[4];
		gsl_write_interface(ddata->client, 0, buf, 4);
		printk("tpd-ps function is off\n");
	}
	return 0;
}

static int tpd_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data *sensor_data;

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value)
				{
					if((tpd_enable_ps(1) != 0))
					{
						printk("enable ps fail: %d\n", err);
						return -1;
					}
				//					set_bit(CMC_BIT_PS, &obj->enable);
				}
				else
				{
					if((tpd_enable_ps(0) != 0))
					{
						printk("disable ps fail: %d\n", err);
						return -1;
					}
				//					clear_bit(CMC_BIT_PS, &obj->enable);
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				printk("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;

				sensor_data->values[0] = tpd_get_ps_value();
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
			break;

		default:
			printk("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;

}
#endif
#ifdef CONFIG_GSL_TP_HALL_MODE
////获取初始灵敏度
static void gsl_gain_hall_data(struct i2c_client *client)
{
	u8 buf[4]={0};
	/**************************/
	buf[0]=0x3;
	gsl_write_interface(client,0xf0,buf,4);
	gsl_read_interface(client,0,&gsl_hall_data[0],4);
	/**************************/
	
	buf[0]=0x7;
	gsl_write_interface(client,0xf0,buf,4);
	gsl_read_interface(client,0x34,&gsl_hall_data[4],4);
	/**************************/
}
//进入皮套模式大阈值从260改为160
static void gsl_enter_hall_mode(struct gsl_ts_data *ts)
{
	u8 buf[4] = {0};
	buf[3] = 0x00;
	buf[2] = 0x00;
	buf[1] = 0x00;
	buf[0] = 0x3;
	gsl_write_interface(ddata->client, 0xf0, buf, 4);
	buf[3] = 0x5a;
	buf[2] = 0x5a;
	buf[1] = 0x5a;
	buf[0] = 0x5a;
	gsl_write_interface(ddata->client, 0, buf, 4);

	buf[3] = 0x00;
	buf[2] = 0x00;
	buf[1] = 0x00;
	buf[0] = 0x7;
	gsl_write_interface(ddata->client, 0xf0, buf, 4);
	buf[3] = 0x00;
	buf[2] = 0x00;
	buf[1] = 0x00;
	buf[0] = 0xa0; //更改后的灵敏度
	gsl_write_interface(ddata->client, 0x34, buf, 4);
	msleep(10);	
	gsl_hall_flag = 0;

}
//退出皮套模式，大阈值从160改为260
static void gsl_quit_hall_mode(struct gsl_ts_data *ts)
{
	u8 buf[4] = {0};
	buf[3] = 0x00;
	buf[2] = 0x00;
	buf[1] = 0x00;
	buf[0] = 0x3;
	gsl_write_interface(ddata->client, 0xf0, buf, 4);
	buf[3] = gsl_hall_data[3];
	buf[2] = gsl_hall_data[2];
	buf[1] = gsl_hall_data[1];
	buf[0] = gsl_hall_data[0];
	gsl_write_interface(ddata->client, 0, buf, 4);

	buf[3] = 0x00;
	buf[2] = 0x00;
	buf[1] = 0x00;
	buf[0] = 0x7;
	gsl_write_interface(ddata->client, 0xf0, buf, 4);
	buf[3] = gsl_hall_data[7];
	buf[2] = gsl_hall_data[6];
	buf[1] = gsl_hall_data[5];
	buf[0] = gsl_hall_data[4];
	gsl_write_interface(ddata->client, 0x34, buf, 4);
	msleep(10);	
	gsl_hall_flag = 1;
}
#endif
#ifdef GSL_GESTURE
static void gsl_enter_doze(struct gsl_ts_data *ts)
{
	u8 buf[4] = {0};
#if 0
	u32 tmp;
	gsl_reset_core(ts->client);
	temp = ARRAY_SIZE(GSLX68X_FW_GESTURE);
	gsl_load_fw(ts->client,GSLX68X_FW_GESTURE,temp);
	gsl_start_core(ts->client);
	msleep(1000);		
#endif
	printk("[tp-gsl]::::gsl_enter_doze\n");

	buf[0] = 0xa;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	gsl_write_interface(ts->client,0xf0,buf,4);
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0x1;
	buf[3] = 0x5a;
	gsl_write_interface(ts->client,0x8,buf,4);
	//gsl_gesture_status = GE_NOWORK;
	msleep(10);
	gsl_gesture_status = GE_ENABLE;

}
static void gsl_quit_doze(struct gsl_ts_data *ts)
{
	u8 buf[4] = {0};
	u32 tmp;

	printk("[tp-gsl]::::gsl_quit_doze\n");
	gsl_gesture_status = GE_DISABLE;
		
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(20);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(5);
	
	buf[0] = 0xa;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	gsl_write_interface(ts->client,0xf0,buf,4);
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0x5a;
	gsl_write_interface(ts->client,0x8,buf,4);
	msleep(10);

#if 0
	gsl_reset_core(ddata->client);
	temp = ARRAY_SIZE(GSLX68X_FW_CONFIG);
	//gsl_load_fw();
	gsl_load_fw(ddata->client,GSLX68X_FW_CONFIG,temp);
	gsl_start_core(ddata->client);
#endif
}

static ssize_t gsl_sysfs_tpgesture_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 count = 0;
	count += scnprintf(buf,PAGE_SIZE,"tp gesture is on/off:\n");
	if(gsl_gesture_flag == 1){
		count += scnprintf(buf+count,PAGE_SIZE-count,
				" on \n");
	}else if(gsl_gesture_flag == 0){
		count += scnprintf(buf+count,PAGE_SIZE-count,
				" off \n");
	}
	count += scnprintf(buf+count,PAGE_SIZE-count,"tp gesture:");
	count += scnprintf(buf+count,PAGE_SIZE-count,
			"%c\n",gsl_gesture_c);
    	return count;
}
static ssize_t gsl_sysfs_tpgesturet_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
#if 1
	if(buf[0] == '0'){
		gsl_gesture_flag = 0;  
	}else if(buf[0] == '1'){
		gsl_gesture_flag = 1;
	}
#endif
	return count;
}
static DEVICE_ATTR(gesture, 0666, gsl_sysfs_tpgesture_show, gsl_sysfs_tpgesturet_store);
#endif

#define GSL_CHIP_NAME	"gslx68x"
static ssize_t gsl_sysfs_version_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	ssize_t len=0;
	int count = 0;
	u32 tmp;
	u8 buf_tmp[4];
	char *ptr = buf;
	count += scnprintf(buf,PAGE_SIZE,"sileadinc:");
	count += scnprintf(buf+count,PAGE_SIZE-count,GSL_CHIP_NAME);

#ifdef GSL_TIMER
	count += scnprintf(buf+count,PAGE_SIZE-count,":0001-1:");
#else
	count += scnprintf(buf+count,PAGE_SIZE-count,":0001-0:");
#endif

#ifdef TPD_PROC_DEBUG
	count += scnprintf(buf+count,PAGE_SIZE-count,"0002-1:");
#else
	count += scnprintf(buf+count,PAGE_SIZE-count,"0002-0:");
#endif

#ifdef TPD_PROXIMITY
	count += scnprintf(buf+count,PAGE_SIZE-count,"0003-1:");
#else
	count += scnprintf(buf+count,PAGE_SIZE-count,"0003-0:");
#endif

#ifdef GSL_DEBUG
	count += scnprintf(buf+count,PAGE_SIZE-count,"0004-1:");
#else
	count += scnprintf(buf+count,PAGE_SIZE-count,"0004-0:");
#endif

#ifdef GSL_ALG_ID
	tmp = gsl_version_id();
	count += scnprintf(buf+count,PAGE_SIZE-count,"%08x:",tmp);
	count += scnprintf(buf+count,PAGE_SIZE-count,"%08x:",
		gsl_cfg_table[gsl_cfg_index].data_id[0]);
#endif
	buf_tmp[0]=0x3;buf_tmp[1]=0;buf_tmp[2]=0;buf_tmp[3]=0;
	gsl_write_interface(ddata->client,0xf0,buf_tmp,4);
	gsl_read_interface(ddata->client,0,buf_tmp,4);
	count += scnprintf(buf+count,PAGE_SIZE-count,"%02x%02x%02x%02x\n",
		buf_tmp[3],buf_tmp[2],buf_tmp[1],buf_tmp[0]);

    return count;
}
static DEVICE_ATTR(version, 0444, gsl_sysfs_version_show, NULL);
static struct attribute *gsl_attrs[] = {
	&dev_attr_version.attr,
#ifdef GSL_GESTURE
	&dev_attr_gesture.attr,
#endif
	NULL
};
static const struct attribute_group gsl_attr_group = {
	.attrs = gsl_attrs,
};
static void gsl_report_point(struct gsl_touch_info *ti)
{
	int tmp = 0;
	static int gsl_up_flag = 0; //prevent more up event
	print_info("gsl_report_point %d \n", ti->finger_num);

	if (unlikely(ti->finger_num == 0)) 
	{
		if(gsl_up_flag == 0)
			return;
	    	gsl_up_flag = 0;
        	input_report_key(tpd->dev, BTN_TOUCH, 0);
        	input_mt_sync(tpd->dev);
		if (FACTORY_BOOT == get_boot_mode()|| 
			RECOVERY_BOOT == get_boot_mode())
		{   

			tpd_button(ti->x[tmp], ti->y[tmp], 0);

		}
	} 
	else 
	{
		gsl_up_flag = 1;
		for (tmp = 0; ti->finger_num > tmp; tmp++) 
		{
			print_info("[gsl1680](x[%d],y[%d]) = (%d,%d);\n", 
				ti->id[tmp], ti->id[tmp], ti->x[tmp], ti->y[tmp]);
			input_report_key(tpd->dev, BTN_TOUCH, 1);
			input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);

			if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
			{ 
				tpd_button(ti->x[tmp], ti->y[tmp], 1);  
			}
			input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, ti->id[tmp] - 1);
			input_report_abs(tpd->dev, ABS_MT_POSITION_X, ti->x[tmp]);
			input_report_abs(tpd->dev, ABS_MT_POSITION_Y, ti->y[tmp]);

			input_mt_sync(tpd->dev);
		}
	}
	input_sync(tpd->dev);
}

static void gsl_report_work(void)
{

	u8 buf[4] = {0};
	u8 i = 0;
	u16 ret = 0;
	u16 tmp = 0;
	struct gsl_touch_info cinfo={0};
	u8 tmp_buf[44] ={0};
#ifdef GSL_GESTURE
	u16 test_count=1;
#endif

#ifdef CONFIG_GSL_TP_HALL_MODE
	u8 j = 0;
	struct gsl_touch_info cinf1={0};
	int slid ;
#endif
	print_info("enter gsl_report_work\n");
#ifdef TPD_PROXIMITY
	int err;
	hwm_sensor_data sensor_data;
    /*added by bernard*/
	if (tpd_proximity_flag == 1)
	{
	
		gsl_read_interface(ddata->client,0xac,buf,4);
		if (buf[0] == 1 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0)
		{
			tpd_proximity_detect = 0;
			//sensor_data.values[0] = 0;
		}
		else
		{
			tpd_proximity_detect = 1;
			//sensor_data.values[0] = 1;
		}
		//get raw data
		print_info(" ps change\n");
		//map and store data to hwm_sensor_data
		sensor_data.values[0] = tpd_get_ps_value();
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
		//let up layer to know
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
			print_info("call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}
	/*end of added*/
#endif

#ifdef TPD_PROC_DEBUG
	if(gsl_proc_flag == 1){
		return;
	}
#endif	

 	gsl_read_interface(ddata->client, 0x80, tmp_buf, 8);
	if(tmp_buf[0]>=2&&tmp_buf[0]<=10)
		gsl_read_interface(ddata->client, 0x88, &tmp_buf[8], (tmp_buf[0]*4-4));
	cinfo.finger_num = tmp_buf[0] & 0x0f;
#ifdef GSL_GESTURE
		printk("GSL:::0x80=%02x%02x%02x%02x[%d]\n",tmp_buf[3],tmp_buf[2],tmp_buf[1],tmp_buf[0],test_count++);
		printk("GSL:::0x84=%02x%02x%02x%02x\n",tmp_buf[7],tmp_buf[6],tmp_buf[5],tmp_buf[4]);
		printk("GSL:::0x88=%02x%02x%02x%02x\n",tmp_buf[11],tmp_buf[10],tmp_buf[9],tmp_buf[8]);
//	if(GE_ENABLE == gsl_gesture_status && gsl_gesture_flag == 1){
//		for(tmp=0;tmp<3;tmp++){
//			printk("tp-gsl-gesture 0x%2x=0x%02x%02x%02x%02x;\n",
//					tmp*4+0x80,tmp_buf[tmp*4+3],tmp_buf[tmp*4+2],tmp_buf[tmp*4+1],
//					tmp_buf[tmp*4]);
//		}
//	}
#endif
	print_info("tp-gsl  finger_num = %d\n",cinfo.finger_num);
	for(tmp=0;tmp<(cinfo.finger_num>10?10:cinfo.finger_num);tmp++)
	{
		cinfo.id[tmp] = tmp_buf[tmp*4+7] >> 4;
		cinfo.y[tmp] = (tmp_buf[tmp*4+4] | ((tmp_buf[tmp*4+5])<<8));
		cinfo.x[tmp] = (tmp_buf[tmp*4+6] | ((tmp_buf[tmp*4+7] & 0x0f)<<8));
		print_info("tp-gsl  x = %d y = %d \n",cinfo.x[tmp],cinfo.y[tmp]);
	}

#ifdef GSL_ALG_ID
	int tmp1 = 0;
	cinfo.finger_num = (tmp_buf[3]<<24)|(tmp_buf[2]<<16)|(tmp_buf[1]<<8)|(tmp_buf[0]);
	gsl_alg_id_main(&cinfo);
	
	tmp1=gsl_mask_tiaoping();
	print_info("[tp-gsl] tmp1=%x\n",tmp1);
	if(tmp1>0&&tmp1<0xffffffff)
	{
		buf[0]=0xa;
		buf[1]=0;
		buf[2]=0;
		buf[3]=0;
		gsl_write_interface(ddata->client,0xf0,buf,4);
		buf[0]=(u8)(tmp1 & 0xff);
		buf[1]=(u8)((tmp1>>8) & 0xff);
		buf[2]=(u8)((tmp1>>16) & 0xff);
		buf[3]=(u8)((tmp1>>24) & 0xff);
		printk("tmp1=%08x,buf[0]=%02x,buf[1]=%02x,buf[2]=%02x,buf[3]=%02x\n",
			tmp1,buf[0],buf[1],buf[2],buf[3]);
		gsl_write_interface(ddata->client,0x8,buf,4);
	}
#endif

#ifdef GSL_GESTURE
	printk("gsl_gesture_status=%c,gsl_gesture_flag=%d[%d]\n",gsl_gesture_status,gsl_gesture_flag,test_count++);

	if(GE_ENABLE == gsl_gesture_status && gsl_gesture_flag == 1){
		int tmp_c;
		u8 key_data = 0;
		tmp_c = gsl_obtain_gesture();
		printk("gsl_obtain_gesture():tmp_c=0x%x[%d]\n",tmp_c,test_count++);
		print_info("gsl_obtain_gesture():tmp_c=0x%x\n",tmp_c);
		switch(tmp_c){
		case (int)'C':
			key_data = KEY_C;
			sprintf(tpgesture_value,"c");
			break;
		case (int)'E':
			key_data = KEY_E;
			sprintf(tpgesture_value,"e");
			break;
		case (int)'W':
			key_data = KEY_W;
			sprintf(tpgesture_value,"w");
			break;
		case (int)'O':
			key_data = KEY_O;
			sprintf(tpgesture_value,"o");
			break;
		case (int)'M':
			key_data = KEY_M;
			sprintf(tpgesture_value,"m");
			break;
		case (int)'Z':
			key_data = KEY_Z;
			sprintf(tpgesture_value,"z");
			break;
		case (int)'V':
			key_data = KEY_V;
			sprintf(tpgesture_value,"v");
			break;
		case (int)'S':
			key_data = KEY_S;
			sprintf(tpgesture_value,"s");
			break;
		case (int)'*':	
			key_data = KEY_POWER;
			sprintf(tpgesture_value,"DOUBCLICK");
			break;/* double click */
        case (int)0xa1fa:
			key_data = KEY_F1;
			sprintf(tpgesture_value,"RIGHT");
			break;/* right */
		case (int)0xa1fd:
			key_data = KEY_F2;
			sprintf(tpgesture_value,"DOWN");
			break;/* down */
		case (int)0xa1fc:	
			key_data = KEY_F3;
			sprintf(tpgesture_value,"UP");
			break;/* up */
		case (int)0xa1fb:	/* left */
			key_data = KEY_F4;
			sprintf(tpgesture_value,"LEFT");
			break;  
		
		}

		if(key_data != 0){
			gsl_gesture_c = (char)(tmp_c & 0xff);
			gsl_gesture_status = GE_WAKEUP;
			printk("gsl_obtain_gesture():tmp_c=%c\n",gsl_gesture_c);

			#if 0
			//input_report_key(tpd->dev,key_data,1);
			input_report_key(tpd->dev,KEY_POWER,1);
			input_sync(tpd->dev);
			//input_report_key(tpd->dev,key_data,0);
		  	input_report_key(tpd->dev,KEY_POWER,0);
			input_sync(tpd->dev);
			#else
			tpgesture_hander();
			#endif
			
		}
		return;
	}
#endif

#ifdef CONFIG_GSL_TP_HALL_MODE
		//add for hall cover
	slid = mt_get_gpio_in(GPIO_QWERTYSLIDE_EINT_PIN);

    printk("[hct-hall]: %s, ---slid=%d\n", __func__, slid);

        
		//print_info("kpd_hall_state=%d\n",kpd_hall_state);
		if(slid==0) //皮套盖上
		{		
			j=0;	
			if(gsl_hall_flag == 1){
				gsl_enter_hall_mode(ddata);
			}
	
					
			for(i=0;i<cinfo.finger_num;i++)
			{
				if((cinfo.y[i]<480)&&((cinfo.x[i]<700)&&(cinfo.x[i]>20)))		  //在皮套窗口范围内
				{
					cinf1.id[j]=cinfo.id[i];
					cinf1.x[j]=cinfo.x[i];
					cinf1.y[j]=cinfo.y[i];
					j++;
				}
			}
			cinf1.finger_num=j;
	
			print_info("gsl_hall_flag=%d\n",gsl_hall_flag);
			print_info("cinf1.finger_num=%d\n",cinf1.finger_num);
			gsl_report_point(&cinf1);
		
		}
		else	 //皮套打开
		{
			if(gsl_hall_flag == 0)
			{
				gsl_quit_hall_mode(ddata);
			}
				
			print_info("gsl_hall_flag=%d\n",gsl_hall_flag);
			print_info("cinf0.finger_num=%d\n",cinfo.finger_num);
			gsl_report_point(&cinfo);	//按正常方式报点
		}
#else      //无皮套模式
	gsl_report_point(&cinfo);
#endif
}


static int touch_event_handler(void *unused)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);
	do
	{
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		tpd_flag = 0;
		TPD_DEBUG_SET_TIME;
		set_current_state(TASK_RUNNING);
		gsl_report_work();
	} while (!kthread_should_stop());	
	return 0;
}

static int tpd_eint_interrupt_handler(void)
{

	print_info("[gsl1680] TPD interrupt has been triggered\n");
	tpd_flag=1; 
    	wake_up_interruptible(&waiter);
}

static void gsl_hw_init(void)
{
	//power on
	hwPowerOn(PMIC_APP_CAP_TOUCH_VDD, VOL_2800, "TP");

	/* reset ctp gsl1680 */
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(20);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	/* set interrupt work mode */
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	msleep(100);
}

static int gsl_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i,err;
	unsigned char tp_data[4];
#ifdef TPD_PROXIMITY
	struct hwmsen_object obj_ps;
#endif

	print_info();

	ddata = kzalloc(sizeof(struct gsl_ts_data), GFP_KERNEL);
	if (!ddata) {
		print_info("alloc ddata memory error\n");
		return -ENOMEM;
	}
	mutex_init(&gsl_i2c_lock);
	ddata->client = client;
	print_info("ddata->client->addr = 0x%x \n",ddata->client->addr);
	gsl_hw_init();

	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	i2c_set_clientdata(ddata->client, ddata);

	err = gsl_test_i2c(ddata->client);
	if(err<0)
		goto  err_malloc;

#ifdef GSL_GESTURE
	gsl_FunIICRead(gsl_read_oneframe_data);
#endif
	gsl_sw_init(ddata->client);
	msleep(20);
	check_mem_data(ddata->client);

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		//err = PTR_ERR(thread);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", (int)PTR_ERR(thread));
	}

//mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
#if 0
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM,
		CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY,
		tpd_eint_interrupt_handler, 1);
#else
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_RISING, tpd_eint_interrupt_handler, 1);
#endif
#ifdef GSL_TIMER
	INIT_DELAYED_WORK(&gsl_timer_check_work, gsl_timer_check_func);
	gsl_timer_workqueue = create_workqueue("gsl_timer_check");
	queue_delayed_work(gsl_timer_workqueue, &gsl_timer_check_work, GSL_TIMER_CHECK_CIRCLE);
#endif
#ifdef TPD_PROC_DEBUG
#if 0
	gsl_config_proc = create_proc_entry(GSL_CONFIG_PROC_FILE, 0666, NULL);
	if (gsl_config_proc == NULL)
	{
		print_info("create_proc_entry %s failed\n", GSL_CONFIG_PROC_FILE);
	}
	else
	{
		gsl_config_proc->read_proc = gsl_config_read_proc;
		gsl_config_proc->write_proc = gsl_config_write_proc;
	}
#else
	proc_create(GSL_CONFIG_PROC_FILE,0666,NULL,&gsl_seq_fops);
#endif
	gsl_proc_flag = 0;
#endif


#ifdef GSL_ALG_ID
	gsl_DataInit(gsl_cfg_table[gsl_cfg_index].data_id);
#endif
#ifdef TPD_PROXIMITY
	//obj_ps.self = gsl1680p_obj;
	//	obj_ps.self = cm3623_obj;
	obj_ps.polling = 0;//interrupt mode
	//obj_ps.polling = 1;//need to confirm what mode is!!!
	obj_ps.sensor_operate = tpd_ps_operate;//gsl1680p_ps_operate;
	if((err = hwmsen_attach(ID_PROXIMITY, &obj_ps)))
	{
		printk("attach fail = %d\n", err);
	}
	
	gsl_gain_psensor_data(ddata->client);
	wake_lock_init(&ps_lock, WAKE_LOCK_SUSPEND, "ps wakelock");
#endif
	//gsl_sysfs_init();
	sysfs_create_group(&client->dev.kobj,&gsl_attr_group);
#ifdef GSL_GESTURE
	input_set_capability(tpd->dev, EV_KEY, KEY_POWER);//×￠2áê?è?×ó?μí3
	input_set_capability(tpd->dev, EV_KEY, KEY_C);
	input_set_capability(tpd->dev, EV_KEY, KEY_E);
	input_set_capability(tpd->dev, EV_KEY, KEY_O);
	input_set_capability(tpd->dev, EV_KEY, KEY_W);
	input_set_capability(tpd->dev, EV_KEY, KEY_M);
	input_set_capability(tpd->dev, EV_KEY, KEY_Z);
	input_set_capability(tpd->dev, EV_KEY, KEY_V);
	input_set_capability(tpd->dev, EV_KEY, KEY_S);
	/*input_set_capability(tpd->dev, EV_KEY, KEY_F1);
	input_set_capability(tpd->dev, EV_KEY, KEY_F2);
	input_set_capability(tpd->dev, EV_KEY, KEY_F3);
	input_set_capability(tpd->dev, EV_KEY, KEY_F4);  */
#endif
#ifdef CONFIG_GSL_TP_HALL_MODE
	gsl_gain_hall_data(ddata->client); //获取初始灵敏度
#endif
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	
	tpd_load_status = 1;

	return 0;

err_malloc:
	if (ddata)
		kfree(ddata);

	return err;
}

/*****************************************************************************
Prototype    : gsl_remove
Description  : remove gsl1680 driver
Input        : struct i2c_client *client
Output       : int
Return Value : static

 *****************************************************************************/
static int gsl_remove(struct i2c_client *client)
{
	print_info("[gsl1680] TPD removed\n");
	return 0;
}

/*****************************************************************************
Prototype    : gsl_detect
Description  : gsl1680 driver local setup without board file
Input        : struct i2c_client *client
int kind
struct i2c_board_info *info
Output       : int
Return Value : static

 *****************************************************************************/
static int gsl_detect (struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	print_info("%s, %d\n", __FUNCTION__, __LINE__);
	strcpy(info->type, TPD_DEVICE);
	return 0;
}

static struct i2c_driver gsl_i2c_driver = {
    .driver = {
		.name = TPD_DEVICE,
		.owner = THIS_MODULE,
    },
	.probe = gsl_probe,
	.remove = gsl_remove,
	.id_table = gsl_device_id,
	.detect = gsl_detect,
};

/*****************************************************************************
Prototype    : gsl_local_init
Description  : setup gsl1680 driver
Input        : None
Output       : None
Return Value : static

 *****************************************************************************/
static int gsl_local_init(void)
{
	int ret;
	print_info();
	boot_mode = get_boot_mode();
	print_info("boot_mode == %d \n", boot_mode);

	if (boot_mode == SW_REBOOT)
	boot_mode = NORMAL_BOOT;

#ifdef TPD_HAVE_BUTTON
	print_info("TPD_HAVE_BUTTON\n");
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);
#endif

	ret = i2c_add_driver(&gsl_i2c_driver);

	if (ret < 0) {
		print_info("unable to i2c_add_driver\n");
		return -ENODEV;
	}

	if (tpd_load_status == 0) 
	{
		print_info("tpd_load_status == 0, gsl_probe failed\n");
		i2c_del_driver(&gsl_i2c_driver);
		return -ENODEV;
	}

	/* define in tpd_debug.h */
	tpd_type_cap = 1;
	print_info("end %s, %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static void gsl_suspend(struct i2c_client *client)
{
	int tmp;
	print_info();
//printk("[tp-gsl]gsl_suspend::::tpd_proximity_flag:%d\n",tpd_proximity_flag);
	
#ifdef TPD_PROXIMITY
	if (tpd_proximity_flag == 1)
	{
	    return 0;
	}
#endif
	gsl_halt_flag = 1;
	//version info
	printk("[tp-gsl]the last time of debug:%x\n",TPD_DEBUG_TIME);
#ifdef GSL_ALG_ID
	tmp = gsl_version_id();	
	printk("[tp-gsl]the version of alg_id:%x\n",tmp);
#endif
	
	//version info
#ifdef GSL_GESTURE
	if(gsl_gesture_flag == 1){
		gsl_enter_doze(ddata);
		return;
	}
#endif
#ifdef TPD_PROC_DEBUG
	if(gsl_proc_flag == 1){
		return;
	}
#endif
#ifdef GSL_TIMER	
	cancel_delayed_work_sync(&gsl_timer_check_work);
#endif


	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
//	gsl_reset_core(ddata->client);
//	msleep(20);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
}

static void gsl_resume(struct i2c_client *client)
{
	print_info();

#ifdef TPD_PROXIMITY
	if (tpd_proximity_flag == 1&&gsl_halt_flag == 0)
	{
		tpd_enable_ps(1);
		return;
	}
#endif


#ifdef TPD_PROC_DEBUG
	if(gsl_proc_flag == 1){
		return;
	}
#endif
#ifdef GSL_GESTURE
		if(gsl_gesture_flag == 1){
			gsl_quit_doze(ddata);
		}
#endif

	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(20);
	gsl_reset_core(ddata->client);
#ifdef GSL_GESTURE	
#ifdef GSL_ALG_ID
	gsl_DataInit(gsl_config_data_id);
#endif
#endif
	gsl_start_core(ddata->client);
	msleep(20);
	check_mem_data(ddata->client);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#ifdef GSL_TIMER
	queue_delayed_work(gsl_timer_workqueue, &gsl_timer_check_work, GSL_TIMER_CHECK_CIRCLE);
#endif
	gsl_halt_flag = 0;
#ifdef TPD_PROXIMITY
	if (tpd_proximity_flag == 1)
	{
		tpd_enable_ps(1);
		return;
	}
#endif

}

#ifdef GSL_GESTURE
static ssize_t show_tpgesture_value(struct device* dev, struct device_attribute *attr, char *buf)
{
	printk("show tp gesture value is %s \n",tpgesture_value);
	return scnprintf(buf, PAGE_SIZE, "%s\n", tpgesture_value);
}
static DEVICE_ATTR(tpgesture,  0664, show_tpgesture_value, NULL);
static struct device_attribute *tpd_attr_list[] = {
	&dev_attr_tpgesture,
};
#endif


static struct tpd_driver_t gsl_driver = {
	.tpd_device_name = GSL_DEV_NAME,
	.tpd_local_init = gsl_local_init,
	.suspend = gsl_suspend,
	.resume = gsl_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
 	.tpd_have_button = 0,
#endif
#ifdef GSL_GESTURE
	.attrs = {
		.attr = &tpd_attr_list,
		.num = (int)(sizeof(tpd_attr_list)/sizeof(tpd_attr_list[0])),
	},
#endif
};

static int __init gsl_driver_init(void)
{
	int ret;

	print_info();
	i2c_register_board_info(I2C_CAP_TOUCH_CHANNEL, &i2c_tpd, 1);
	if(ret = tpd_driver_add(&gsl_driver) < 0)
		print_info("gsl_driver init error, return num is %d \n", ret);

	return ret;
}

static void __exit gsl_driver_exit(void)
{
	print_info();
	tpd_driver_remove(&gsl_driver);
}

module_init(gsl_driver_init);
module_exit(gsl_driver_exit);

