#define FUNC(name) \
	.global name; \
	.hidden name; \
	.type name,@function; \
	.align 16; \
	name:

#define ENDF(name) \
	.size name,.-name

#if defined(_WIN32) || defined(__CYGWIN__)
#define r_arg0 %rcx
#define r_arg1 %rdx
#define r_arg2 %r8
#define r_arg3 %r9
#define r_arg4 %rsi
#define r_arg5 %rdi
#define e_arg0 %ecx
#define e_arg1 %edx
#define e_arg2 %r8d
#define e_arg3 %r9d
#define e_arg4 %esi
#define e_arg5 %edi
#else
#define r_arg0 %rdi
#define r_arg1 %rsi
#define r_arg2 %rdx
#define r_arg3 %rcx
#define r_arg4 %r8
#define r_arg5 %r9
#define e_arg0 %edi
#define e_arg1 %esi
#define e_arg2 %edx
#define e_arg3 %ecx
#define e_arg4 %r8d
#define e_arg5 %r9d
#endif

#define r_src      r_arg0
#define r_srcLast5 r_arg1
#define r_out0     r_arg2
#define r_out      r_arg3

#define L(name) .L##name

FUNC(RELZ4_decompress)
	add r_src,r_srcLast5
	mov r_out0,r_out
	sub $5,r_srcLast5

	.align 16
L(loop):
	movzb (r_src),%r10d
	mov %r10d,%eax
	shr $4,%eax
	cmp $0xf0,%r10d
	jae L(bigllen)
	movups 1(r_src),%xmm0
	add $1,r_src
	movups %xmm0,(r_out)
	add %rax,r_src
	add %rax,r_out
	mov r_out,%rax
	cmp r_srcLast5,r_src
	ja L(break)
L(match):
	movzw (r_src),%r11d
	and $15,%r10d
	jz L(bigmlen)
	sub %r11,%rax
	movups -16(%rax),%xmm0
	movups %xmm0,(r_out)
	add $2,r_src
	movzw (%rax),%eax
	movw %ax,16(r_out)
	add %r10,r_out
	add $3,r_out
	jmp L(loop)

	.align 16
L(bigllen):
	movzb 1(r_src),%eax
	cmp $240,%eax
	jae L(bigllen2)
	add $2,r_src
L(gotllen):
	add $15,%eax
	add r_out,%rax

	.align 16
L(copylloop):
	movups (r_src),%xmm0
	movups 16(r_src),%xmm1
	add $32,r_src
	movups %xmm0,(r_out)
	movups %xmm1,16(r_out)
	add $32,r_out
	cmp %rax,r_out
	jb L(copylloop)

	sub %rax,r_out
	sub r_out,r_src
	mov %rax,r_out

	cmp r_srcLast5,r_src
	jbe L(match)
	jmp L(break)

	.align 16
L(bigmlen):
	movzb 2(r_src),%eax
	cmp $240,%eax
	jae L(bigmlen2)
	add $3,r_src
L(gotmlen):
	add $20,%rax
	neg %r11
	add r_out,%rax

	.align 16
L(copymloop):
	movups -16(r_out,%r11),%xmm0
	movups %xmm0,(r_out)
	movups (r_out,%r11),%xmm1
	movups %xmm1,16(r_out)
	add $32,r_out
	cmp %rax,r_out
	jb L(copymloop)
	mov %rax,r_out
	jmp L(loop)

L(bigllen2):
	movzb 2(r_src),%r11d
	shl $4,%r11d
	add %r11d,%eax
	cmp $224*16,%r11d
	jae L(bigllen3)
	add $3,r_src
	jmp L(gotllen)
L(bigmlen2):
	movzb 3(r_src),%r10d
	shl $4,%r10d
	add %r10d,%eax
	cmp $224*16,%r10d
	jae L(bigmlen3)
	add $4,r_src
	jmp L(gotmlen)

L(bigllen3):
	movzb 3(r_src),%r11d
	shl $9,%r11d
	add %r11d,%eax
	cmp $129*16*32,%r11d
	jae L(bigllen4)
	add $4,r_src
	jmp L(gotllen)
L(bigmlen3):
	movzb 4(r_src),%r10d
	shl $9,%r10d
	add %r10d,%eax
	cmp $129*16*32,%r10d
	jae L(bigmlen4)
	add $5,r_src
	jmp L(gotmlen)

L(bigllen4):
	movzw 4(r_src),%r11d
	add $6,r_src
	imul $16*32*127,%r11d,%r11d
	add %r11d,%eax
	jmp L(gotllen)
L(bigmlen4):
	movzw 5(r_src),%r10d
	add $7,r_src
	imul $16*32*127,%r10d,%r10d
	add %r10d,%eax
	jmp L(gotmlen)

L(break):
	sub r_out0,%rax
	ret
ENDF(RELZ4_decompress)
