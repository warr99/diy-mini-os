/**
 * 功能：16位与32位的启动混合代码
 * 相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 */
	#include "os.h"

	// 声明本地以下符号是全局的，在其它源文件中可以访问
	.global _start,timer_int,syscall_handler

	// 指定以下的代码生成16位的机器指令,这样才能在启动时的实模式下运行
  	.code16

	// 以下是代码区
 	.text
_start: 
	// 设置段寄存器
	jmp $0, $offset
offset:
	mov $0, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov $_start, %esp


read_self_all:
	mov $_start_32, %bx
	mov $0x2, %cx
	mov $0x240, %ax
	mov $0x80, %dx
	int $0x13
	jc read_self_all

	cli
	lgdt gdt_desc
	lidt idt_desc
	mov $1, %eax
	lmsw %ax
	jmp $KERNEL_CODE_SEG, $_start_32

	// 指示程序将从内存地址 0x1fe (510) 开始执行
	.org 0x1fe 
	// 标识引导扇区的结束
	.byte 0x55, 0xaa

	.code32
	.text
_start_32:
		mov $KERNEL_DATA_SEG, %ax
		mov %ax, %ds
		mov %ax, %es
		mov %ax, %ss
		mov $_start, %esp

		call os_init
		
		mov $pg_dir, %eax
		mov %eax, %cr3

		mov %cr4, %eax
		orl $(1 << 4), %eax
		mov %eax, %cr4

		mov %cr0, %eax
		orl $(1 << 31), %eax
		mov %eax, %cr0

		mov $TASK0_TSS_SEG, %ax
		ltr %ax 

		mov $TASK0_LDT_SEG, %ax
		lldt %ax


		push $TASK_DATA_SEG
		push $task0_dpl3_stack + 1024 * 4
		push $0x202
		push $TASK_CODE_SEG
		push $task_0_entry
		iret

task_0_entry:
	mov %ss, %ax
	mov %ax, %ds
	mov %ax, %es
	jmp task_0			// 跳转到任务0运行
		
timer_int:
	push %ds
	pusha 

	mov $0x20, %al
	outb %al, $0x20

	mov $KERNEL_DATA_SEG, %ax
	mov %ax, %ds
	call task_sched

	popa
	pop %ds

	iret

syscall_handler:
	push %ds
	pusha 

	mov $KERNEL_DATA_SEG, %ax
	mov %ax, %ds

	mov %esp ,%ebp
	push 13 * 4(%ebp)
	push 12 * 4(%ebp)
	push 11 * 4(%ebp)
	call do_syscall
	add $(3*4), %esp

	popa
	pop %ds

	retf $(3*4)


gdt_desc:
	.word (256 * 8) - 1
	.long gdt_table

idt_desc:
	.word (256 * 8) - 1
	.long idt_table
