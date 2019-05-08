/*
 * This file generates the __sys.bjou module to STDOUT.
 *
 * Brandon Kammerdiener
 * Feb 1, 2019
 */

#include <stdio.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define PRINT_SYS_NR_DECL(NR) printf("const "#NR" := SYS_NR_MOD + %d\n", (NR))
#define PRINT_SYS_CONSTANT_DECL(C) printf("const "#C" := %d\n", (C))

void pre() {
    printf(
"# __sys.bjou\n"
"# This file was generated by gen_sys.\n"
"# system calls\n"
"\n"
"module __sys\n"
"\n"
"import \"os.bjou\"\n"
"\n"
"extern nolibc_syscall(i64, i32, ...) : i32\n"
"\n"
"\\static_if{ os::OS != os::MACOS\n"
"    const SYS_NR_MOD := 0x0 }\n"
"\\static_if{ os::OS == os::MACOS\n"
"    const SYS_NR_MOD := 0x2000000 }\n"
"\n"
"type fd_t        = int\n"
"type mode_t      = u16\n"
"type time_t      = i64\n"
"type suseconds_t = i32\n"
"\n"
"type timeval {\n"
"    tv_sec  : time_t\n"
"    tv_usec : suseconds_t\n"
"}\n"
"type timezone {\n"
"    tz_minuteswest : i32\n"
"    tz_dsttime     : i32\n"
"}\n"
"\n");
}

void post() {
    printf(
"\n"
"proc write(fd : i32, buf : void*, nbyte : u64) : i64\n"
"    return nolibc_syscall(SYS_write, 3, fd, buf, nbyte)\n"
"\n"
"proc read(fd : i32, buf : void*, nbyte : u64) : i64\n"
"    return nolibc_syscall(SYS_read, 3, fd, buf, nbyte)\n"
"\n"
"proc open(path: char*, oflag : int) : i64\n"
"    return nolibc_syscall(SYS_open, 2, path, oflag)\n"
"\n"
"proc open(path: char*, oflag : int, mode : mode_t) : i64\n"
"    return nolibc_syscall(SYS_open, 3, path, oflag, mode)\n"
"\n"
"proc close(fd : int) : i64\n"
"    return nolibc_syscall(SYS_close, 1, fd)\n"
"\n"
"proc exit(status : int) : i64\n"
"    return nolibc_syscall(SYS_exit, 1, status)\n"
"\n"
"proc lseek(fd : int, offset : u64, whence : int) : u64\n"
"    return nolibc_syscall(SYS_lseek, 3, fd, offset, whence)\n"
"\n"
"proc access(path : char*, mode : int) : i64\n"
"    return nolibc_syscall(SYS_access, 2, path, mode)\n"
"\n"
"proc getpid() : i64\n"
"    return nolibc_syscall(SYS_getpid, 0)\n"
"\n"
"proc gettimeofday(tv : timeval*, tz : timezone*) : i64\n"
"    return nolibc_syscall(SYS_gettimeofday, 2, tv, tz)\n");
}

int main() {
    pre();

    PRINT_SYS_NR_DECL(SYS_write);
    PRINT_SYS_NR_DECL(SYS_read);
    PRINT_SYS_NR_DECL(SYS_open);
    PRINT_SYS_NR_DECL(SYS_close);
    PRINT_SYS_NR_DECL(SYS_exit);
    PRINT_SYS_NR_DECL(SYS_lseek);
    PRINT_SYS_NR_DECL(SYS_access);
    PRINT_SYS_NR_DECL(SYS_getpid);
    PRINT_SYS_NR_DECL(SYS_gettimeofday);

    PRINT_SYS_CONSTANT_DECL(S_IRWXU);
    PRINT_SYS_CONSTANT_DECL(S_IRUSR);
    PRINT_SYS_CONSTANT_DECL(S_IWUSR);
    PRINT_SYS_CONSTANT_DECL(S_IXUSR);
    PRINT_SYS_CONSTANT_DECL(S_IRWXG);
    PRINT_SYS_CONSTANT_DECL(S_IRGRP);
    PRINT_SYS_CONSTANT_DECL(S_IWGRP);
    PRINT_SYS_CONSTANT_DECL(S_IXGRP);
    PRINT_SYS_CONSTANT_DECL(S_IRWXO);
    PRINT_SYS_CONSTANT_DECL(S_IROTH);
    PRINT_SYS_CONSTANT_DECL(S_IWOTH);
    PRINT_SYS_CONSTANT_DECL(S_IXOTH);
    PRINT_SYS_CONSTANT_DECL(O_RDONLY);
    PRINT_SYS_CONSTANT_DECL(O_WRONLY);
    PRINT_SYS_CONSTANT_DECL(O_RDWR);
    PRINT_SYS_CONSTANT_DECL(O_APPEND);
    PRINT_SYS_CONSTANT_DECL(O_CREAT);
    PRINT_SYS_CONSTANT_DECL(O_TRUNC);
    PRINT_SYS_CONSTANT_DECL(O_EXCL);
    PRINT_SYS_CONSTANT_DECL(SEEK_SET);
    PRINT_SYS_CONSTANT_DECL(SEEK_CUR);
    PRINT_SYS_CONSTANT_DECL(SEEK_END);
    PRINT_SYS_CONSTANT_DECL(F_OK);

    post();

    return 0;
}

