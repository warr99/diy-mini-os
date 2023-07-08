/**
 * 功能：32位代码，完成多任务的运行
 */
#include "os.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

void task_0(void)
{
    uint8_t color = 0;
    for (;;)
    {
        color++;
    }
}

void task_1(void)
{
    uint8_t color = 0xff;
    for (;;)
    {
        color--;
    }
}

#define MAP_ADDR (0x80000000)
#define PDE_P (1 << 0)
#define PDE_W (1 << 1)
#define PDE_U (1 << 2)
#define PDE_PS (1 << 7)

uint8_t map_phy_buffer[4096] __attribute__((aligned(4096))) = {0x36};

static uint32_t pg_table[1024] __attribute__((aligned(4096))) = {PDE_U}; // 要给个值，否则其实始化值不确定

uint32_t pg_dir[1024] __attribute__((aligned(4096))) = {
    [0] = (0) | PDE_P | PDE_PS | PDE_W | PDE_U, // PDE_PS，开启4MB的页，恒等映射
};

uint32_t task0_dpl3_stack[1024];

struct
{
    uint16_t offset_l, selector, attr, offset_h;
} idt_table[256] __attribute__((aligned(8)));

struct
{
    uint16_t limit_l, base_l, basehl_attr, base_limit;
} gdt_table[256] __attribute__((aligned(8))) = {
    // 0x00cf9a000000ffff - 从0地址开始，P存在，DPL=0，Type=非系统段，32位代码段（非一致代码段），界限4G，
    [KERNEL_CODE_SEG / 8] = {0xffff, 0x0000, 0x9a00, 0x00cf},
    // 0x00cf93000000ffff - 从0地址开始，P存在，DPL=0，Type=非系统段，数据段，界限4G，可读写
    [KERNEL_DATA_SEG / 8] = {0xFFFF, 0x0000, 0x9200, 0x00cf},
    [APP_CODE_SEG / 8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    [APP_DATA_SEG / 8] = {0xFFFF, 0x0000, 0xf300, 0x00cf},
};

void outb(uint8_t data, uint16_t port)
{
    __asm__ __volatile__("outb %[v], %[p]" ::[p] "d"(port), [v] "a"(data));
}
void timer_int(void);

void os_init(void)
{
    outb(0x11, 0x20);
    outb(0x11, 0xA0);
    outb(0x20, 0x21);
    outb(0x28, 0xA1);
    outb(1 << 2, 0x21);
    outb(2, 0xa1);
    outb(0x1, 0x21);
    outb(0x1, 0xA1);
    outb(0xfe, 0x21);
    outb(0xff, 0xa1);

    int tmo = 1193180 / 100;
    outb(0x36, 0x43);
    outb((uint16_t)tmo, 0x40);
    outb(tmo >> 8, 0x40);

    idt_table[0x20].offset_l = (uint32_t)timer_int & 0xFFFF;
    idt_table[0x20].offset_h = (uint32_t)timer_int >> 16;
    idt_table[0x20].selector = KERNEL_CODE_SEG;
    idt_table[0x20].attr = 0x8E00;

    pg_dir[MAP_ADDR >> 22] = (uint32_t)pg_table | PDE_P | PDE_W | PDE_U;
    pg_table[(MAP_ADDR >> 12) & 0x3FF] = (uint32_t)map_phy_buffer | PDE_P | PDE_W | PDE_U;
};
