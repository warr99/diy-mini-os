/**
 * 功能：32位代码，完成多任务的运行
 */
#include "os.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

void do_syscall(int func, char* str, char color) {
    static int row = 1;  // 初始值不能为0，否则其初始化值不确定

    if (func == 2) {
        // 显示器共80列，25行，按字符显示，每个字符需要用两个字节表示
        unsigned short* dest = (unsigned short*)0xb8000 + 80 * row;
        while (*str) {
            // 其中一个字节保存要显示的字符，另一个字节表示颜色
            *dest++ = *str++ | (color << 8);
        }

        // 逐行显示，超过一行则回到第0行再显示
        row = (row >= 25) ? 0 : row + 1;

        // 加点延时，让显示慢下来
        for (int i = 0; i < 0xFFFFFF; i++)
            ;
    }
}

void sys_show(char* str, char color) {
    uint32_t addr[] = {0, SYSCALL_SEG};
    __asm__ __volatile__("push %[color]; push %[str]; push %[id]; lcalll *(%[a])" ::[a] "r"(addr), [color] "m"(color), [str] "m"(str), [id] "r"(2));
}

void task_0(void) {
    uint8_t color = 0;
    char* str = "task_0: hello, syscall";
    for (;;) {
        sys_show(str, color++);
    }
}

void task_1(void) {
    uint8_t color = 0xff;
    char* str = "task_1: hello, syscall";
    for (;;) {
        sys_show(str, color--);
    }
}

#define MAP_ADDR (0x80000000)
#define PDE_P (1 << 0)
#define PDE_W (1 << 1)
#define PDE_U (1 << 2)
#define PDE_PS (1 << 7)

uint8_t map_phy_buffer[4096] __attribute__((aligned(4096))) = {0x36};

static uint32_t pg_table[1024] __attribute__((aligned(4096))) = {PDE_U};  // 要给个值，否则其实始化值不确定

uint32_t pg_dir[1024] __attribute__((aligned(4096))) = {
    [0] = (0) | PDE_P | PDE_PS | PDE_W | PDE_U,  // PDE_PS，开启4MB的页，恒等映射
};

uint32_t task0_dpl0_stack[1024], task0_dpl3_stack[1024], task1_dpl0_stack[1024], task1_dpl3_stack[1024];

struct {
    uint16_t limit_l, base_l, basehl_attr, base_limit;
} task0_ldt_table[256] __attribute__((aligned(8))) = {
    [TASK_CODE_SEG / 8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    [TASK_DATA_SEG / 8] = {0xFFFF, 0x0000, 0xf300, 0x00cf},
};

struct {
    uint16_t limit_l, base_l, basehl_attr, base_limit;
} task1_ldt_table[256] __attribute__((aligned(8))) = {
    [TASK_CODE_SEG / 8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    [TASK_DATA_SEG / 8] = {0xFFFF, 0x0000, 0xf300, 0x00cf},
};

uint32_t task0_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,
    (uint32_t)task0_dpl0_stack + 4 * 1024,
    KERNEL_DATA_SEG,
    /* 后边不用使用 */ 0x0,
    0x0,
    0x0,
    0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,
    (uint32_t)task_0 /*入口地址*/,
    0x202,
    0xa,
    0xc,
    0xd,
    0xb,
    (uint32_t)task0_dpl3_stack + 4 * 1024 /* 栈 */,
    0x1,
    0x2,
    0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    TASK_DATA_SEG,
    TASK_CODE_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK0_LDT_SEG,
    0x0,
};

uint32_t task1_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,
    (uint32_t)task1_dpl0_stack + 4 * 1024,
    KERNEL_DATA_SEG,
    /* 后边不用使用 */ 0x0,
    0x0,
    0x0,
    0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,
    (uint32_t)task_1 /*入口地址*/,
    0x202,
    0xa,
    0xc,
    0xd,
    0xb,
    (uint32_t)task1_dpl3_stack + 4 * 1024 /* 栈 */,
    0x1,
    0x2,
    0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    TASK_DATA_SEG,
    TASK_CODE_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK1_LDT_SEG,
    0x0,
};

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

    [TASK0_TSS_SEG / 8] = {0x68, 0, 0xe900, 0x0},
    [TASK1_TSS_SEG / 8] = {0x68, 0, 0xe900, 0x0},
    [SYSCALL_SEG / 8] = {0x0000, KERNEL_CODE_SEG, 0xec03, 0},

    [TASK0_LDT_SEG / 8] = {sizeof(task0_ldt_table) - 1, 0x00, 0xe200, 0x00cf},
    [TASK1_LDT_SEG / 8] = {sizeof(task1_ldt_table) - 1, 0x00, 0xe200, 0x00cf},
};

void outb(uint8_t data, uint16_t port) {
    __asm__ __volatile__("outb %[v], %[p]" ::[p] "d"(port), [v] "a"(data));
}

void task_sched(void) {
    static int task_tss = TASK0_TSS_SEG;
    task_tss = (task_tss == TASK0_TSS_SEG) ? TASK1_TSS_SEG : TASK0_TSS_SEG;
    uint32_t addr[] = {0, task_tss};
    __asm__ __volatile__("ljmpl *(%[a])" ::[a] "r"(addr));
}

void timer_int(void);
void syscall_handler(void);

void os_init(void) {
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

    gdt_table[TASK0_TSS_SEG / 8].base_l = (uint16_t)(uint32_t)task0_tss;
    gdt_table[TASK1_TSS_SEG / 8].base_l = (uint16_t)(uint32_t)task1_tss;
    gdt_table[SYSCALL_SEG / 8].limit_l = (uint16_t)(uint32_t)syscall_handler;
    gdt_table[TASK0_LDT_SEG / 8].base_l = (uint32_t)task0_ldt_table;
    gdt_table[TASK1_LDT_SEG / 8].base_l = (uint32_t)task1_ldt_table;

    pg_dir[MAP_ADDR >> 22] = (uint32_t)pg_table | PDE_P | PDE_W | PDE_U;
    pg_table[(MAP_ADDR >> 12) & 0x3FF] = (uint32_t)map_phy_buffer | PDE_P | PDE_W | PDE_U;
};
