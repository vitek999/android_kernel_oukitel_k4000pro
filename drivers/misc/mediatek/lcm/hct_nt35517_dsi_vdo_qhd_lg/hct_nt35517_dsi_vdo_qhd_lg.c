#ifdef BUILD_LK
#else
#include <linux/string.h>
#if defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_gpio.h>
#endif
#endif
#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (540)
#define FRAME_HEIGHT (960)

#define REGFLAG_DELAY             							0xAB
#define REGFLAG_END_OF_TABLE      							0xAA	// END OF REGISTERS MARKER

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

#define LCM_ID       (0x5517)
#define LCM_ID1       (0xC1)
#define LCM_ID2       (0x80)

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)									lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)				lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_sleep_out_setting[] = {
	// Sleep Out
	{0x11, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	// Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_initialization_setting[] = {
/*
 {0xFF,4,{0xAA,0x55,0x25,0x01}},
	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x00}},
 {0xB3,1,{0x00}},
//Forward Scan      CTB=CRL=0

 {0xB1,2,{0xFC,0x00}},

// Source hold time
//{0xB6,0x05}},

// Gate EQ control

// Source EQ control (Mode 2)
 {0xB8,4,{0x01,0x04,0x05,0x06}},

// Bias Current

// Inversion

//#Frame Rate

// Display Timing: Dual 8-phase 4-overlap
 {0xC9,6,{0x63,0x06,0x0D,0x1A,0x17,0x00}},//6c
 {0xBC,3,{0x02,0x00,0x00}},

//*************************************
// Select CMD2, Page 1
//*************************************
	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x01}},

// AVDD: 5.3V
 {0xB0,3,{0x05,0x05,0x05}},

// AVEE: -5.3V
 {0xB1,1,{0x05,0x05,0x05}},

// VCL: -3.5V
	{0xB2, 3, {0x02, 0x02, 0x02}},

// VGH: 15.0V
 {0xB3, 3,{0x0d,0x0d,0x0d}},//0e

// VGLX: -10.0V
 {0xB4,3,{0x09,0x09,0x09}},//09

// AVDD: 3xVDDB
 {0xB6, 3,{0x43,0x43,0x43}},//34

// AVEE: -2xVDDB
 {0xB7,3,{0x33,0x33,0x33}},

// VCL: -2xVDDB
 {0xB8,3,{0x34,0x34,0x34}},//15

// VGH: 2xAVDD-AVEE
 {0xB9,3,{0x26,0x26,0x26}},//22

// VGLX: AVEE-AVDD
 {0xBA,3,{0x26,0x26,0x26}},//22

// Set VGMP = 4.9V / VGSP = 0V

// Set VGMN = -4.9V / VGSN = 0V

// VMSEL 0: 0xBE  ;  1 : 0xBF 

// Set VCOM_offset

// Pump:0x00 or PFM:0x50 control

// Gamma Gradient Control
//R(+) MCR cmd



//G(+) MCR cmd
{0xBC,3,{0x00,0x86,0x00}},
{0xBD,3,{0x00,0x86,0x00}},
  {0xBf,1,{0x50}},//5A,0x55,0x58
  {0xBe,1,{0x50}},
{0xC0,2,{0x04,0x00}},
 {0xCA,1,{0x00}},
//R(+) MCR cmd
 {0xD1,16,{0x00,0x16,0x00,0x21,0x00,0x37,0x00,0x4c,0x00,0x61,0x00,0x8c,0x00,0xa9,0x00,0xd6}},
 {0xD2,16,{0x00,0xF0,0x01,0x36,0x01,0x65,0x01,0xAF,0x01,0xEF,0x01,0xF1,0x02,0x28,0x02,0x6B}},
 {0xD3,16,{0x02,0x90,0x02,0xC3,0x02,0xE6,0x03,0x18,0x03,0x3A,0x03,0x69,0x03,0x84,0x03,0xAC}},
 {0xD4,4, {0x03,0xE3,0x03,0xFF}},
 {0xD5,16,{0x00,0x16,0x00,0x21,0x00,0x37,0x00,0x4c,0x00,0x61,0x00,0x8c,0x00,0xa9,0x00,0xd6}},
 {0xD6,16,{0x00,0xF0,0x01,0x36,0x01,0x65,0x01,0xAF,0x01,0xEF,0x01,0xF1,0x02,0x28,0x02,0x6B}},
 {0xD7,16,{0x02,0x90,0x02,0xC3,0x02,0xE6,0x03,0x18,0x03,0x3A,0x03,0x69,0x03,0x84,0x03,0xAC}},
 {0xD8,4, {0x03,0xE3,0x03,0xFF}},
 {0xD9,16,{0x00,0x16,0x00,0x21,0x00,0x37,0x00,0x4c,0x00,0x61,0x00,0x8c,0x00,0xa9,0x00,0xd6}},
 {0xDD,16,{0x00,0xF0,0x01,0x36,0x01,0x65,0x01,0xAF,0x01,0xEF,0x01,0xF1,0x02,0x28,0x02,0x6B}},
 {0xDE,16,{0x02,0x90,0x02,0xC3,0x02,0xE6,0x03,0x18,0x03,0x3A,0x03,0x69,0x03,0x84,0x03,0xAC}},
 {0xDF,4, {0x03,0xE3,0x03,0xFF}},
 {0xE0,16,{0x00,0x16,0x00,0x21,0x00,0x37,0x00,0x4c,0x00,0x61,0x00,0x8c,0x00,0xa9,0x00,0xd6}},
 {0xE1,16,{0x00,0xF0,0x01,0x36,0x01,0x65,0x01,0xAF,0x01,0xEF,0x01,0xF1,0x02,0x28,0x02,0x6B}},
 {0xE2,16,{0x02,0x90,0x02,0xC3,0x02,0xE6,0x03,0x18,0x03,0x3A,0x03,0x69,0x03,0x84,0x03,0xAC}},
 {0xE3,4, {0x03,0xE3,0x03,0xFF}},
 {0xE4,16,{0x00,0x16,0x00,0x21,0x00,0x37,0x00,0x4c,0x00,0x61,0x00,0x8c,0x00,0xa9,0x00,0xd6}},
 {0xE5,16,{0x00,0xF0,0x01,0x36,0x01,0x65,0x01,0xAF,0x01,0xEF,0x01,0xF1,0x02,0x28,0x02,0x6B}},
 {0xE6,16,{0x02,0x90,0x02,0xC3,0x02,0xE6,0x03,0x18,0x03,0x3A,0x03,0x69,0x03,0x84,0x03,0xAC}},
 {0xE7,4, {0x03,0xE3,0x03,0xFF}},
 {0xE8,16,{0x00,0x16,0x00,0x21,0x00,0x37,0x00,0x4c,0x00,0x61,0x00,0x8c,0x00,0xa9,0x00,0xd6}},

 {0xE9,16,{0x00,0xF0,0x01,0x36,0x01,0x65,0x01,0xAF,0x01,0xEF,0x01,0xF1,0x02,0x28,0x02,0x6B}},
 {0xEA,16,{0x02,0x90,0x02,0xC3,0x02,0xE6,0x03,0x18,0x03,0x3A,0x03,0x69,0x03,0x84,0x03,0xAC}},
 {0xEB,4, {0x03,0xE3,0x03,0xFF}},
{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
*/

//B(+) MCR cmd



//R(-) MCR cmd



//G(-) MCR cmd



//B(-) MCR cmd



/*
//R(+) MCR cmd
{ 0xD1,16,{0x00,0x22,0x00,0x24,0x00,0x2F,0x00,0x52,0x00,0x79,0x00,0xB7,0x00,0xE7,0x01,0x25}},
{ 0xD2,16,{0x01,0x53,0x01,0x93,0x01,0xC0,0x02,0x00,0x02,0x30,0x02,0x31,0x02,0x5C,0x02,0x85}},
{ 0xD3,16,{0x02,0x9C,0x02,0xB7,0x02,0xC7,0x02,0xDC,0x02,0xE9,0x02,0xFB,0x03,0x08,0x03,0x1B}},
{ 0xD4,4,{0x03,0x41,0x03,0x5D}},
//G(+) MCR cmd
{ 0xD5,16,{0x00,0x22,0x00,0x24,0x00,0x2F,0x00,0x52,0x00,0x79,0x00,0xB7,0x00,0xE7,0x01,0x25}},
{ 0xD6,16,{0x01,0x53,0x01,0x93,0x01,0xC0,0x02,0x00,0x02,0x30,0x02,0x31,0x02,0x5C,0x02,0x85}},
{ 0xD7,16,{0x02,0x9C,0x02,0xB7,0x02,0xC7,0x02,0xDC,0x02,0xE9,0x02,0xFB,0x03,0x08,0x03,0x1B}},
{ 0xD8,4,{0x03,0x41,0x03,0x5D}},
//B(+) MCR cmd
{ 0xD9,16,{0x00,0x22,0x00,0x24,0x00,0x2F,0x00,0x52,0x00,0x79,0x00,0xB7,0x00,0xE7,0x01,0x25}},
{ 0xDD,16,{0x01,0x53,0x01,0x93,0x01,0xC0,0x02,0x00,0x02,0x30,0x02,0x31,0x02,0x5C,0x02,0x85}},
{ 0xDE,16,{0x02,0x9C,0x02,0xB7,0x02,0xC7,0x02,0xDC,0x02,0xE9,0x02,0xFB,0x03,0x08,0x03,0x1B}},
{ 0xDF,4,{0x03,0x41,0x03,0x5D}},
//R(-) MCR cmd
{ 0xE0,16,{0x00,0x22,0x00,0x24,0x00,0x2F,0x00,0x52,0x00,0x79,0x00,0xB7,0x00,0xE7,0x01,0x25}},
{ 0xE1,16,{0x01,0x53,0x01,0x93,0x01,0xC0,0x02,0x00,0x02,0x30,0x02,0x31,0x02,0x5C,0x02,0x85}},
{ 0xE2,16,{0x02,0x9C,0x02,0xB7,0x02,0xC7,0x02,0xDC,0x02,0xE9,0x02,0xFB,0x03,0x08,0x03,0x1B}},
{ 0xE3,4,{0x03,0x41,0x03,0x5D}},
//G(-) MCR cmd
{ 0xE4,16,{0x00,0x22,0x00,0x24,0x00,0x2F,0x00,0x52,0x00,0x79,0x00,0xB7,0x00,0xE7,0x01,0x25}},
{ 0xE5,16,{0x01,0x53,0x01,0x93,0x01,0xC0,0x02,0x00,0x02,0x30,0x02,0x31,0x02,0x5C,0x02,0x85}},
{ 0xE6,16,{0x02,0x9C,0x02,0xB7,0x02,0xC7,0x02,0xDC,0x02,0xE9,0x02,0xFB,0x03,0x08,0x03,0x1B}},
{ 0xE7,4,{0x03,0x41,0x03,0x5D}},
//B(-) MCR cmd
{ 0xE8,16,{0x00,0x22,0x00,0x24,0x00,0x2F,0x00,0x52,0x00,0x79,0x00,0xB7,0x00,0xE7,0x01,0x25}},
{ 0xE9,16,{0x01,0x53,0x01,0x93,0x01,0xC0,0x02,0x00,0x02,0x30,0x02,0x31,0x02,0x5C,0x02,0x85}},
{ 0xEA,16,{0x02,0x9C,0x02,0xB7,0x02,0xC7,0x02,0xDC,0x02,0xE9,0x02,0xFB,0x03,0x08,0x03,0x1B}},
{ 0xEB,4,{0x03,0x41,0x03,0x5D}},
*/
//*************************************
// Select CMD3 Enable
//*************************************
/*
{ 0xFF,4,{ 0xAA, 0x55, 0x25, 0x01}},

{ 0xF3,7,{0x02,0x03,0x07,0x44,0x88,0xD1,0x0D}},
*/

//*************************************
// TE On                               
//*************************************
//{0x35,1,{0x00}},

//*************************************
// Sleep Out
//*************************************


{0xF0, 5,{0x55, 0xAA, 0x52, 0x08, 0x00}},
{0xB1, 3,{0xFC, 0x04, 0x00}},
{0xB3, 1,{0x00}},
{0xB8, 4,{0x01, 0x03, 0x03, 0x03}},
{0xBB, 3,{0x53, 0x03, 0x53}},
{0xBC, 1,{0x02}},
{0xC9, 6,{0x67, 0x06, 0x0D, 0x1A, 0x17, 0x00}},

//-----,{-----------------------------
//D2, P,{age 1
//-----,{-----------------------------
{0xF0, 5,{0x55, 0xAA, 0x52, 0x08, 0x01}},
{0xB0, 3,{0x05, 0x05, 0x05}},
{0xB1, 3,{0x05, 0x05, 0x05}},
{0xB2, 3,{0x02, 0x02, 0x02}},
{0xB3, 3,{0x0e, 0x0e, 0x0e}},
{0xB4, 3,{0x08, 0x08, 0x08}},
{0xB6, 3,{0x44, 0x44, 0x44}},
{0xB7, 3,{0x34, 0x34, 0x34}},
{0xB8, 3,{0x24, 0x24, 0x24}},
{0xB9, 3,{0x26, 0x26, 0x26}},//24
{0xBA, 3,{0x24, 0x24, 0x24}},
{0xBC, 3,{0x00, 0x98, 0x00}},//80
{0xBD, 3,{0x00, 0x98, 0x00}},//80
{0xBE, 1,{0x50}},//52 63 5B 55
{0xC0, 2,{0x00, 0x08}},
//{0xCA, 1,{0x00}},
{0xCF, 1,{0x00}},

//{0xD0, 4,{0x0A, 0x10, 0x0D, 0x0F}},

{0xD1, 16,{0x00, 0x47, 0x00, 0x60, 0x00, 0x81, 0x00, 0x9b, 0x00, 0xac, 0x00, 0xce, 0x00, 0xeb, 0x01, 0x13}},
{0xD2, 16,{0x01, 0x34, 0x01, 0x6b, 0x01, 0x97, 0x01, 0xd9, 0x02, 0x0f, 0x02, 0x10, 0x02, 0x46, 0x02, 0x82}},
{0xD3, 16,{0x02, 0xa8, 0x02, 0xdf, 0x03, 0x03, 0x03, 0x2d, 0x03, 0x48, 0x03, 0x61, 0x03, 0x62, 0x03, 0x63}},
{0xD4, 4, {0x03, 0x78, 0x03, 0x7b}},

{0xD5, 16,{0x00, 0x4c, 0x00, 0x60, 0x00, 0x7f, 0x00, 0x97, 0x00, 0xab, 0x00, 0xcc, 0x00, 0xe7, 0x01, 0x13}},
{0xD6, 16,{0x01, 0x34, 0x01, 0x69, 0x01, 0x94, 0x01, 0xd7, 0x02, 0x0d, 0x02, 0x0f, 0x02, 0x45, 0x02, 0x81}},
{0xD7, 16,{0x02, 0xa8, 0x02, 0xdc, 0x02, 0xff, 0x03, 0x30, 0x03, 0x4f, 0x03, 0x78, 0x03, 0x9d, 0x03, 0xe6}},
{0xD8, 4, {0x03, 0xfe, 0x03, 0xFe}},

{0xD9, 16,{0x00, 0x52, 0x00, 0x60, 0x00, 0x78, 0x00, 0x8d, 0x00, 0x9f, 0x00, 0xbe, 0x00, 0xda, 0x01, 0x07}},
{0xDD, 16,{0x01, 0x29, 0x01, 0x5f, 0x01, 0x8c, 0x01, 0xd1, 0x02, 0x08, 0x02, 0x0a, 0x02, 0x42, 0x02, 0x7d}},
{0xDE, 16,{0x02, 0xa4, 0x02, 0xDA, 0x02, 0xFF, 0x03, 0x38, 0x03, 0x66, 0x03, 0xFB, 0x03, 0xFC, 0x03, 0xFD}},
{0xDF, 4, {0x03, 0xFE, 0x03, 0xFE}},

{0xE0, 16,{0x00, 0x47, 0x00, 0x60, 0x00, 0x81, 0x00, 0x9b, 0x00, 0xac, 0x00, 0xce, 0x00, 0xeb, 0x01, 0x13}},
{0xE1, 16,{0x01, 0x34, 0x01, 0x6b, 0x01, 0x97, 0x01, 0xd9, 0x02, 0x0f, 0x02, 0x10, 0x02, 0x46, 0x02, 0x82}},
{0xE2, 16,{0x02, 0xa8, 0x02, 0xdf, 0x03, 0x03, 0x03, 0x2d, 0x03, 0x48, 0x03, 0x61, 0x03, 0x62, 0x03, 0x63}},
{0xE3, 4, {0x03, 0x78, 0x03, 0x7b}},

{0xE4, 16,{0x00, 0x4c, 0x00, 0x60, 0x00, 0x7f, 0x00, 0x97, 0x00, 0xab, 0x00, 0xcc, 0x00, 0xe7, 0x01, 0x13}},
{0xE5, 16,{0x01, 0x34, 0x01, 0x69, 0x01, 0x94, 0x01, 0xd7, 0x02, 0x0d, 0x02, 0x0f, 0x02, 0x45, 0x02, 0x81}},
{0xE6, 16,{0x02, 0xa8, 0x02, 0xdc, 0x02, 0xff, 0x03, 0x30, 0x03, 0x4f, 0x03, 0x78, 0x03, 0x9d, 0x03, 0xe6}},
{0xE7, 4, {0x03, 0xfe, 0x03, 0xFe}},

{0xE8, 16,{0x00, 0x52, 0x00, 0x60, 0x00, 0x78, 0x00, 0x8d, 0x00, 0x9f, 0x00, 0xbe, 0x00, 0xda, 0x01, 0x07}},
{0xE9, 16,{0x01, 0x29, 0x01, 0x5f, 0x01, 0x8c, 0x01, 0xd1, 0x02, 0x08, 0x02, 0x0a, 0x02, 0x42, 0x02, 0x7d}},
{0xEA, 16,{0x02, 0xa4, 0x02, 0xDA, 0x02, 0xFF, 0x03, 0x38, 0x03, 0x66, 0x03, 0xFB, 0x03, 0xFC, 0x03, 0xFD}},
{0xEB, 4, {0x03, 0xFE, 0x03, 0xFE}},

{0xFF, 4,{0xAA, 0x55, 0x25, 0x01}},
{0x6F, 1,{0x0A}},
{0xFA, 1,{0x03}},
       
{0x35, 1,{0x00}},

	{0x11, 1, {0x00}},

	{REGFLAG_DELAY, 120, {}},
//*************************************
// Display On
//*************************************
	{0x29, 1, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 10, {}},

	// Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);                
			/*           
			   if (cmd != 0xFF && cmd != 0x2C && cmd != 0x3C) {
			   //#if defined(BUILD_UBOOT)
			   //  printf("[DISP] - uboot - REG_R(0x%x) = 0x%x. \n", cmd, table[i].para_list[0]);
			   //#endif
			   while(read_reg(cmd) != table[i].para_list[0]);              
			   }
			 */
		}
	}
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS * util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS * params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

    params->type   = LCM_TYPE_DSI;    params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	// enable tearing-free
	params->dbi.te_mode = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity = LCM_POLARITY_FALLING;

//    params->dsi.mode   = SYNC_PULSE_VDO_MODE;
	params->dsi.mode = SYNC_EVENT_VDO_MODE;

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_TWO_LANE;

	//The following defined the fomat for data coming from LCD engine.

	params->dsi.intermediat_buffer_num = 2;	//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.word_count = 540 * 3;	//DSI CMD mode need set these two bellow params, different to 6577
	params->dsi.vertical_active_line = 960;
	params->dsi.compatibility_for_nvk = 0;	// this parameter would be set to 1 if DriverIC is NTK's and when force match DSI clock for NTK's
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 4;	//4 30 30 4 80 80 205 
	params->dsi.vertical_backporch = 30;	//16
	params->dsi.vertical_frontporch = 30;	//15
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 4;	//10
	params->dsi.horizontal_backporch				= 80;//64 80 80
	params->dsi.horizontal_frontporch				= 80;//64
      //params->dsi.horizontal_blanking_pixel                  = 60;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	// Bit rate calculation
#if 0
	params->dsi.pll_div1=0;
	params->dsi.pll_div2=1;		// div2=0,1,2,3;div2_real=1,2,4,4
	params->dsi.fbk_div=18;//13
#else
	params->dsi.PLL_CLOCK=230;//227;//254;//254//247//225
#endif
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
}

static void init_lcm_registers(void)
{

/*nt35510 hsd fwvga init add by hct-mjw 20130603*/
	unsigned int data_array[16];

	//*************Enable TE  *******************//
	data_array[0] = 0x00053902;
	data_array[1] = 0x2555aaff;
	data_array[2] = 0x00000001;
	dsi_set_cmdq(data_array, 3, 1);

    data_array[0]= 0x00093902;
    data_array[1]= 0x000201f8;
    data_array[2]= 0x00133320;
    data_array[3]= 0x00000048;
    dsi_set_cmdq(data_array, 4, 1);
	//*************Enable CMD2 Page1 *******************//
	data_array[0] = 0x00063902;
	data_array[1] = 0x52aa55f0;
	data_array[2] = 0x00000108;
	dsi_set_cmdq(data_array, 3, 1);

	//************* AVDD: manual  *******************//
	data_array[0] = 0x00043902;
    data_array[1]=0x0d0d0db0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
    data_array[1]=0x343434b6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
    data_array[1]=0x0d0d0db1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
    data_array[1]=0x343434b7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
    data_array[1]=0x000000b2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
    data_array[1]=0x242424b8;
	dsi_set_cmdq(data_array, 2, 1);
    data_array[0]=0x00023902;
    data_array[1]=0x000001bf;
    dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00043902;
    data_array[1]=0x0f0f0fb3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
    data_array[1]=0x343434b9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
    data_array[1]=0x080808b5;
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0]=0x00023902;
    data_array[1]=0x000003c2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x242424ba;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
    data_array[1]=0x007800bc;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
    data_array[1]=0x007800bd;
	dsi_set_cmdq(data_array, 2, 1);


	data_array[0] = 0x00033902;
    data_array[1]=0x006400be;
	dsi_set_cmdq(data_array, 2, 1);


	//*************Gamma Table  *******************//
    data_array[0]=0x00353902;
    data_array[1]=0x003300D1;
    data_array[2]=0x003A0034;
    data_array[3]=0x005C004A;
    data_array[4]=0x00A60081;

    data_array[5]=0x011301E5;
    data_array[6]=0x01820154;
    data_array[7]=0x020002CA;
    data_array[8]=0x02340201;
    data_array[9]=0x02840267;

    data_array[10]=0x02B702A4;
    data_array[11]=0x02DE02CF;
    data_array[12]=0x03FE02F2;
    data_array[13]=0x03330310;
    data_array[14]=0x0000006D;
    dsi_set_cmdq(data_array, 15, 1);

    data_array[0]=0x00353902;
    data_array[1]=0x003300D2;
    data_array[2]=0x003A0034;
    data_array[3]=0x005C004A;
    data_array[4]=0x00A60081;

    data_array[5]=0x011301E5;
    data_array[6]=0x01820154;
    data_array[7]=0x020002CA;
    data_array[8]=0x02340201;
    data_array[9]=0x02840267;

    data_array[10]=0x02B702A4;
    data_array[11]=0x02DE02CF;
    data_array[12]=0x03FE02F2;
    data_array[13]=0x03330310;
    data_array[14]=0x0000006D;
    dsi_set_cmdq(data_array, 15, 1);

    data_array[0]=0x00353902;
    data_array[1]=0x003300D3;
    data_array[2]=0x003A0034;
    data_array[3]=0x005C004A;
    data_array[4]=0x00A60081;

    data_array[5]=0x011301E5;
    data_array[6]=0x01820154;
    data_array[7]=0x020002CA;
    data_array[8]=0x02340201;
    data_array[9]=0x02840267;

    data_array[10]=0x02B702A4;
    data_array[11]=0x02DE02CF;
    data_array[12]=0x03FE02F2;
    data_array[13]=0x03330310;
    data_array[14]=0x0000006D;
    dsi_set_cmdq(data_array, 15, 1);

    data_array[0]=0x00353902;
    data_array[1]=0x003300D4;
    data_array[2]=0x003A0034;
    data_array[3]=0x005C004A;

    data_array[4]=0x00A60081;
    data_array[5]=0x011301E5;
    data_array[6]=0x01820154;
    data_array[7]=0x020002CA;
    data_array[8]=0x02340201;
    data_array[9]=0x02840267;

    data_array[10]=0x02B702A4;
    data_array[11]=0x02DE02CF;
    data_array[12]=0x03FE02F2;
    data_array[13]=0x03330310;
    data_array[14]=0x0000006D;
    dsi_set_cmdq(data_array, 15, 1);

    data_array[0]=0x00353902;
    data_array[1]=0x003300D5;
    data_array[2]=0x003A0034;
    data_array[3]=0x005C004A;
    data_array[4]=0x00A60081;
    data_array[5]=0x011301E5;

    data_array[6]=0x01820154;
    data_array[7]=0x020002CA;
    data_array[8]=0x02340201;
    data_array[9]=0x02840267;

    data_array[10]=0x02B702A4;
    data_array[11]=0x02DE02CF;
    data_array[12]=0x03FE02F2;
    data_array[13]=0x03330310;
    data_array[14]=0x0000006D;
    dsi_set_cmdq(data_array, 15, 1);

    data_array[0]=0x00353902;
    data_array[1]=0x003300D6;
    data_array[2]=0x003A0034;
    data_array[3]=0x005C004A;
    data_array[4]=0x00A60081;

    data_array[5]=0x011301E5;
    data_array[6]=0x01820154;
    data_array[7]=0x020002CA;
    data_array[8]=0x02340201;
    data_array[9]=0x02840267;

    data_array[10]=0x02B702A4;
    data_array[11]=0x02DE02CF;
    data_array[12]=0x03FE02F2;
    data_array[13]=0x03330310;
    data_array[14]=0x0000006D;
    dsi_set_cmdq(data_array, 15, 1);
	MDELAY(10);






	// ********************  EABLE CMD2 PAGE 0 **************//
	data_array[0] = 0x00063902;
	data_array[1] = 0x52aa55f0;
	data_array[2] = 0x00000008;
	dsi_set_cmdq(data_array, 3, 1);

	// ********************  EABLE DSI TE **************//
	data_array[0] = 0x00033902;
	data_array[1] = 0x0000fcb1;
	dsi_set_cmdq(data_array, 2, 1);

    data_array[0]=0x00023902;
    data_array[1]=0x00006bb5;
    dsi_set_cmdq(data_array, 2, 1);    
    data_array[0]=0x00023902;
    data_array[1]=0x000005b6;
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0]=0x00033902;
    data_array[1]=0x007070b7;
    dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00053902;
    data_array[1]=0x030301b8;
    data_array[2]=0x00000003;
	dsi_set_cmdq(data_array, 3, 1);

    data_array[0]=0x00043902;
    data_array[1]=0x000002bc;
    dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00063902;
    data_array[1]=0x5002d0c9;

    data_array[2]=0x00005050;
	dsi_set_cmdq(data_array, 3, 1);

	// ********************  EABLE DSI TE packet **************//
	data_array[0] = 0x00351500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x773a1500;
	dsi_set_cmdq(data_array, 1, 1);
    data_array[0]= 0x00053902;
    data_array[1]= 0x0100002a;
    data_array[2]= 0x000000df;
    dsi_set_cmdq(data_array, 3, 1);
    data_array[0]= 0x00053902;
    data_array[1]= 0x0300002b;
    data_array[2]= 0x00000055;
    dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
    MDELAY(5);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
    MDELAY(100);

    //init_lcm_registers();
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];


	data_array[0] = 0x00280500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);

    data_array[0] = 0x00100500;
	dsi_set_cmdq(data_array, 1, 1);
    MDELAY(30);
}

static void lcm_resume(void)
{
	//static unsigned int flicker=0x50;
	//flicker+=2;
	//lcm_initialization_setting[15].para_list[1]=flicker;
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y,
		       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
    data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
    data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

static void lcm_setbacklight(unsigned int level)
{
	unsigned int data_array[16];

#if defined(BUILD_LK)
	printf("%s, %d\n", __func__, level);
#elif defined(BUILD_UBOOT)
	printf("%s, %d\n", __func__, level);
#else
	printk("lcm_setbacklight = %d\n", level);
#endif

	if (level > 255)
		level = 255;

	data_array[0] = 0x00023902;
	data_array[1] = (0x51 | (level << 8));
	dsi_set_cmdq(data_array, 2, 1);
}

static void lcm_setpwm(unsigned int divider)
{
	// TBD
}

static unsigned int lcm_getpwm(unsigned int divider)
{
	// ref freq = 15MHz, B0h setting 0x80, so 80.6% * freq is pwm_clk;
	// pwm_clk / 255 / 2(lcm_setpwm() 6th params) = pwm_duration = 23706
	unsigned int pwm_clk = 23706 / (1 << divider);

	return pwm_clk;
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0, id2 = 0;
	unsigned int id3 = 0;
	unsigned char buffer[2];
	unsigned int data_array[16];

	SET_RESET_PIN(1);	//NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);

	/*  
	   data_array[0] = 0x00110500;              // Sleep Out
	   dsi_set_cmdq(data_array, 1, 1);
	   MDELAY(120);
	 */

	//*************Enable CMD2 Page1  *******************//
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000108;
	dsi_set_cmdq(data_array, 3, 1);
	MDELAY(10);

	data_array[0] = 0x00023700;	// read id return two byte,version and id
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);

	read_reg_v2(0xC5, buffer, 2);
	id2 = buffer[0];	//we only need ID
	id3 = buffer[1];	//we test buffer 1
//    return 1;
	id = id2 << 8 | id3;
	return (LCM_ID == id) ? 1 : 0;
}

static unsigned int lcm_esd_check(void)
{
  #ifndef BUILD_LK
	char  buffer[3];
	int   array[4];
	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0x36, buffer, 1);
	printk("esd: read data =%x \n",buffer[0]);
	if(buffer[0]==0x90)
	{
		return 0;
	}
	else
	{
		return 1;
	}
 #endif
}
static unsigned int lcm_esd_recover(void)
{
    lcm_init();
    return 1;
}
// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER hct_nt35517_dsi_vdo_qhd_lg = 
{
    .name			= "hct_nt35517_dsi_vdo_qhd_lg",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	//.set_backlight = lcm_setbacklight,
	//.set_pwm        = lcm_setpwm,
	//.get_pwm        = lcm_getpwm,
	.compare_id = lcm_compare_id,
    .esd_check = lcm_esd_check,
    .esd_recover = lcm_esd_recover,
	//.update         = lcm_update
};