/*
 * nolibc_syscall.c
 *
 * x86_64 system call interface that is not a part
 * of or dependent upon libc.
 *
 * Improved with help from https://github.com/streamich/libsys
 *
 * Brandon Kammerdiener
 * October, 2018
 */

#include <stdarg.h>

#ifdef __APPLE__
    #define CARRY_FLAG_BIT 1
    #define RETURN_SYSCALL_RESULT(result, flags) return (flags & CARRY_FLAG_BIT) ? -result : result;

    #define MV_R10    "movq %6, %%r10;\n"
    #define MV_R8     "movq %7, %%r8;\n"
    #define MV_R9     "movq %8, %%r9;\n"
    #define MV_RESULT "movq %%r11, %1;\n"
    #define OUTPUTS   "=a" (result), "=r" (flags)
#else
    #define RETURN_SYSCALL_RESULT(result, flags) return result;

    #define MV_R10    "movq %5, %%r10;\n"
    #define MV_R8     "movq %6, %%r8;\n"
    #define MV_R9     "movq %7, %%r9;\n"
    #define MV_RESULT
    #define OUTPUTS   "=a" (result)
#endif

static inline long nolibc_syscall6(long n, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6) {
    long result;
    long flags;

    __asm__ __volatile__ (
        MV_R10
        MV_R8
        MV_R9
        "syscall;\n"
        MV_RESULT
        : OUTPUTS
        : "a" (n), "D" (arg1), "S" (arg2), "d" (arg3), "r" (arg4), "r" (arg5), "r" (arg6)
        : "%r10", "%r8", "%r9", "%rcx", "%r11"
    );

    RETURN_SYSCALL_RESULT(result, flags);
}

static inline long nolibc_syscall5(long n, long arg1, long arg2, long arg3, long arg4, long arg5) {
    long result;
    long flags;

    __asm__ __volatile__ (
        MV_R10
        MV_R8
        "syscall;\n"
        MV_RESULT
        : OUTPUTS
        : "a" (n), "D" (arg1), "S" (arg2), "d" (arg3), "r" (arg4), "r" (arg5)
        : "%r10", "%r8", "%rcx", "%r11"
    );

    RETURN_SYSCALL_RESULT(result, flags);
}

static inline long nolibc_syscall4(long n, long arg1, long arg2, long arg3, long arg4) {
    long result;
    long flags;

    __asm__ __volatile__ (
        MV_R10
        "syscall;\n"
        MV_RESULT
        : OUTPUTS
        : "a" (n), "D" (arg1), "S" (arg2), "d" (arg3), "r" (arg4)
        : "%r10", "%rcx", "%r11"
    );

    RETURN_SYSCALL_RESULT(result, flags);
}

static inline long nolibc_syscall3(long n, long arg1, long arg2, long arg3) {
    long result;
    long flags;

    __asm__ __volatile__ (
        "syscall;\n"
        MV_RESULT
        : OUTPUTS
        : "a" (n), "D" (arg1), "S" (arg2), "d" (arg3)
        : "%rcx", "%r11"
    );

    RETURN_SYSCALL_RESULT(result, flags);
}

static inline long nolibc_syscall2(long n, long arg1, long arg2) {
    long result;
    long flags;

    __asm__ __volatile__ (
        "syscall;\n"
        MV_RESULT
        : OUTPUTS
        : "a" (n), "D" (arg1), "S" (arg2)
        : "%rcx", "%r11"
    );

    RETURN_SYSCALL_RESULT(result, flags);
}

static inline long nolibc_syscall1(long n, long arg1) {
    long result;
    long flags;

    __asm__ __volatile__ (
        "syscall;\n"
        MV_RESULT
        : OUTPUTS
        : "a" (n), "D" (arg1)
        : "%rcx", "%r11"
    );

    RETURN_SYSCALL_RESULT(result, flags);
}

static inline long nolibc_syscall0(long n) {
    long result;
    long flags;

    __asm__ __volatile__ (
        "syscall;\n"
        MV_RESULT
        : OUTPUTS
        : "a" (n)
        : "%rcx", "%r11"
    );

    RETURN_SYSCALL_RESULT(result, flags);
}

long int nolibc_syscall(long int n, int n_args, ...) {
    va_list args;
    long int r, a1, a2, a3, a4, a5, a6;

    va_start(args, n_args);

    switch (n_args) {
        case 0:
            r = nolibc_syscall0(n);
            break;
        case 1:
            r = nolibc_syscall1(n, va_arg(args, long int));
            break;
        case 2:
            a1 = va_arg(args, long int);
            a2 = va_arg(args, long int);
            r = nolibc_syscall2(n, a1, a2);
            break;
        case 3:
            a1 = va_arg(args, long int);
            a2 = va_arg(args, long int);
            a3 = va_arg(args, long int);
            r = nolibc_syscall3(n, a1, a2, a3);
            break;
        case 4:
            a1 = va_arg(args, long int);
            a2 = va_arg(args, long int);
            a3 = va_arg(args, long int);
            a4 = va_arg(args, long int);
            r = nolibc_syscall4(n, a1, a2, a3, a4);
            break;
        case 5:
            a1 = va_arg(args, long int);
            a2 = va_arg(args, long int);
            a3 = va_arg(args, long int);
            a4 = va_arg(args, long int);
            a5 = va_arg(args, long int);
            r = nolibc_syscall5(n, a1, a2, a3, a4, a5);
            break;
        case 6:
            a1 = va_arg(args, long int);
            a2 = va_arg(args, long int);
            a3 = va_arg(args, long int);
            a4 = va_arg(args, long int);
            a5 = va_arg(args, long int);
            a6 = va_arg(args, long int);
            r = nolibc_syscall6(n, a1, a2, a3, a4, a5, a6);
            break;
        default:
            r = -1;
    }

    va_end(args);

    return r;
}
