//
// Created by ubuntu on 11/28/17.
//

#ifndef UBIANALYSIS_CONFIG_H
#define UBIANALYSIS_CONFIG_H
#include <stack>
#include "Common.h"
#define _ID 1
#define _UD 0
// Setup functions for heap allocations.
static void SetHeapAllocFuncs(
        std::set<std::string> &HeapAllocFuncs){

    std::string HeapAllocFN[] = {
        "__kmalloc",
	"kzalloc",
        "__vmalloc",
        "vmalloc",
        "krealloc",
        "devm_kzalloc",
        "vzalloc",
        "malloc",
        "kmem_cache_alloc",
        "__alloc_skb",
	"devm_kmalloc",
    };

    for (auto F : HeapAllocFN) {
        HeapAllocFuncs.insert(F);
    }
}
static void SetPreSumFuncs(
        std::set<std::string> &PreSumFuncs){

    std::string PreSumFN[] = {
        "printk",
        "mutex_unlock",
        "kfree",
        "dev_err",
	"mutex_lock_nested",
	"_raw_spin_unlock_irqrestore",
	"_raw_spin_lock_irqsave",
	"__dynamic_dev_dbg",
	"lockdep_rcu_suspicious",
	"__dynamic_pr_debug",
	"_raw_spin_unlock",
	"_raw_spin_lock",
	"sprintf",
	"dev_warn",
	"snprintf",
	"_dev_info",
	"rcu_read_lock_sched_held",
	"_raw_spin_unlock_bh",
	"_raw_spin_lock_bh",
	"_raw_spin_unlock_irq",
	"_raw_spin_lock_irq",
	"libcfs_debug_msg",
	"__wake_up",
	"__might_sleep",
	"dev_printk",
	"rcu_read_lock_held",
	"kfree_skb",
	"lockdep_init_map",
	"seq_printf",
	"lbug_with_loc",
	"netdev_err",
	"platform_driver_unregister",
	"__init_waitqueue_head",
	"__platform_driver_register",
	"__init_work",
	"trace_seq_printf",
	"__dynamic_netdev_dbg",
	"trace_define_field",
	"trace_raw_output_prep",
	"finish_wait",
	"usleep_range",
	"init_timer_key",
	"lock_acquire",
	"lock_release",
	"free_irq",
	"schedule",
	"mod_timer",
	"queue_work_on",
	"prepare_to_wait_event",
	"consume_skb",
	"__might_fault",
	"complete",
	"dump_page",
	"clk_unprepare",
	"scnprintf",
	"iounmap",
	"up_write",
    };

    for (auto F : PreSumFN) {
        PreSumFuncs.insert(F);
    }
}

static void SetZeroMallocFuncs(
	std::set<std::string> &ZeroMallocFuncs) {
	std::string ZeroAllocFN[] = {
		"kzalloc",
	};
	for (auto F : ZeroAllocFN) {
            ZeroMallocFuncs.insert(F);
        }
}
static void SetTransferFuncs(
		std::set<std::string> &TransferFuncs) {
    std::string TransferFN[] = {
        "copy_to_user",
        "__copy_to_user",
	"_copy_to_user",
        "copy_from_user",
        "__copy_from_user",
	"_copy_from_user",
    };
    for (auto F : TransferFN) {
        TransferFuncs.insert(F);
    }
}
static void SetCopyFuncs(
		std::set<std::string> &CopyFuncs) {
    std::string CopyFN[] = {
        "memcpy",
	    "llvm.memcpy.p0i8.p0i8.i32",
	    "llvm.memcpy.p0i8.p0i8.i64",
	    "memmove",
	    "llvm.memmove.p0i8.p0i8.i32",
	    "llvm.memmove.p0i8.p0i8.i64",
    };
	for (auto F : CopyFN) {
        CopyFuncs.insert(F);
    }
}
static void SetInitFuncs(
        std::set<std::string> &InitFuncs){
    std::string InitFN[] = {
	"llvm.va_start",
        "memset",
        "llvm.memset.p0i8.i32",
        "llvm.memset.p0i8.i64",
    };
    for (auto F : InitFN) {
        InitFuncs.insert(F);
    }
}
static void SetOtherFuncs(
        std::set<std::string> &OtherFuncs){
    std::string OtherFN[] = {
        "mcount",
        "llvm.dbg.declare",
	"llvm.dbg.value",
	"llvm.dbg.addr",
    };
    for (auto F : OtherFN) {
        OtherFuncs.insert(F);
    }
}
static void SetObjSizeFuncs(
        std::set<std::string> &ObjSizeFuncs){
    std::string ObjSizeFN[] = {
        "llvm.objectsize.i64.p0i8",
    };
    for (auto F : ObjSizeFN) {
        ObjSizeFuncs.insert(F);
    }
}
static void SetStrFuncs(
        std::set<std::string> &StrFuncs){
    std::string StrFN[] = {
        "strcmp",
	"strncmp",
	"strcpy",
	"strlwr",
	"strcat",
	"strlen",
	"strupr",
	"strrchr",
	"strncat",
    };
    for (auto F : StrFN) {
        StrFuncs.insert(F);
    }
}
//10 candidates of the inderect caller which could be checked by casting
static void SetIndirectFuncs(
        std::set<std::string> &indirectFuncs){
    std::string IndirectFN[] = {
	"kvm_vfio_group_is_coherent",
        //"kvm_vfio_external_group_match_file",
	//"kvm_device_ioctl_attr",
	//"kvm_device_ioctl",
	//"kvm_destroy_devices",
	//"sctp_inq_push",
	//"kvm_arch_vcpu_unblocking",
	//"kvm_vcpu_ioctl_set_cpuid2",
	//"hda_call_check_power_status",
	//"restore_mixer_state",
    };
    for (auto F : IndirectFN) {
        indirectFuncs.insert(F);
    }
}
static void SetSyscallFuncs(
        std::set<std::string> &syscallFuncs){
    std::string SyscallFN[] = {
		"__se_sys_io_setup",
		"__se_sys_io_destroy",
"__se_sys_io_submit",
"__se_sys_io_cancel",
"__se_sys_io_getevents",
"__se_sys_io_getevents_time32",
"__se_sys_io_pgetevents",
"__se_sys_io_pgetevents_time32",
"__se_sys_io_uring_setup",
"__se_sys_io_uring_enter",
"__se_sys_io_uring_register",
"__se_sys_setxattr",
"__se_sys_lsetxattr",
"__se_sys_fsetxattr",
"__se_sys_getxattr",
"__se_sys_lgetxattr",
"__se_sys_fgetxattr",
"__se_sys_listxattr",
"__se_sys_llistxattr",
"__se_sys_flistxattr",
"__se_sys_removexattr",
"__se_sys_lremovexattr",
"__se_sys_fremovexattr",
"__se_sys_getcwd",
"__se_sys_lookup_dcookie",
"__se_sys_eventfd2",
"__se_sys_epoll_create1",
"__se_sys_epoll_ctl",
"__se_sys_epoll_pwait",
"__se_sys_dup",
"__se_sys_dup3",
"__se_sys_fcntl",
"__se_sys_fcntl64",
"__se_sys_inotify_init1",
"__se_sys_inotify_add_watch",
"__se_sys_inotify_rm_watch",
"__se_sys_ioctl",
"__se_sys_ioprio_set",
"__se_sys_ioprio_get",
"__se_sys_flock",
"__se_sys_mknodat",
"__se_sys_mkdirat",
"__se_sys_unlinkat",
"__se_sys_symlinkat",
"__se_sys_linkat",
"__se_sys_renameat",
"__se_sys_umount",
"__se_sys_mount",
"__se_sys_pivot_root",
"__se_sys_statfs",
"__se_sys_statfs64",
"__se_sys_fstatfs",
"__se_sys_fstatfs64",
"__se_sys_truncate",
"__se_sys_ftruncate",
"__se_sys_truncate64",
"__se_sys_ftruncate64",
"__se_sys_fallocate",
"__se_sys_faccessat",
"__se_sys_chdir",
"__se_sys_fchdir",
"__se_sys_chroot",
"__se_sys_fchmod",
"__se_sys_fchmodat",
"__se_sys_fchownat",
"__se_sys_fchown",
"__se_sys_openat",
"__se_sys_openat2",
"__se_sys_close",
"__se_sys_vhangup",
"__se_sys_pipe2",
"__se_sys_quotactl",
"__se_sys_getdents64",
"__se_sys_llseek",
"__se_sys_lseek",
"__se_sys_read",
"__se_sys_write",
"__se_sys_readv",
"__se_sys_writev",
"__se_sys_pread64",
"__se_sys_pwrite64",
"__se_sys_preadv",
"__se_sys_pwritev",
"__se_sys_sendfile64",
"__se_sys_pselect6",
"__se_sys_pselect6_time32",
"__se_sys_ppoll",
"__se_sys_ppoll_time32",
"__se_sys_signalfd4",
"__se_sys_vmsplice",
"__se_sys_splice",
"__se_sys_tee",
"__se_sys_readlinkat",
"__se_sys_newfstatat",
"__se_sys_newfstat",
"__se_sys_fstat64",
"__se_sys_fstatat64",
"__se_sys_sync",
"__se_sys_fsync",
"__se_sys_fdatasync",
"__se_sys_sync_file_range2",
"__se_sys_sync_file_range",
"__se_sys_timerfd_create",
"__se_sys_timerfd_settime",
"__se_sys_timerfd_gettime",
"__se_sys_timerfd_gettime32",
"__se_sys_timerfd_settime32",
"__se_sys_utimensat",
"__se_sys_utimensat_time32",
"__se_sys_acct",
"__se_sys_capget",
"__se_sys_capset",
"__se_sys_personality",
"__se_sys_exit",
"__se_sys_exit_group",
"__se_sys_waitid",
"__se_sys_set_tid_address",
"__se_sys_unshare",
"__se_sys_futex",
"__se_sys_futex_time32",
"__se_sys_get_robust_list",
"__se_sys_set_robust_list",
"__se_sys_nanosleep",
"__se_sys_nanosleep_time32",
"__se_sys_getitimer",
"__se_sys_setitimer",
"__se_sys_kexec_load",
"__se_sys_init_module",
"__se_sys_delete_module",
"__se_sys_timer_create",
"__se_sys_timer_gettime",
"__se_sys_timer_getoverrun",
"__se_sys_timer_settime",
"__se_sys_timer_delete",
"__se_sys_clock_settime",
"__se_sys_clock_gettime",
"__se_sys_clock_getres",
"__se_sys_clock_nanosleep",
"__se_sys_timer_gettime32",
"__se_sys_timer_settime32",
"__se_sys_clock_settime32",
"__se_sys_clock_gettime32",
"__se_sys_clock_getres_time32",
"__se_sys_clock_nanosleep_time32",
"__se_sys_syslog",
"__se_sys_ptrace",
"__se_sys_sched_setparam",
"__se_sys_sched_setscheduler",
"__se_sys_sched_getscheduler",
"__se_sys_sched_getparam",
"__se_sys_sched_setaffinity",
"__se_sys_sched_getaffinity",
"__se_sys_sched_yield",
"__se_sys_sched_get_priority_max",
"__se_sys_sched_get_priority_min",
"__se_sys_sched_rr_get_interval",
"__se_sys_sched_rr_get_interval_time32",
"__se_sys_restart_syscall",
"__se_sys_kill",
"__se_sys_tkill",
"__se_sys_tgkill",
"__se_sys_sigaltstack",
"__se_sys_rt_sigsuspend",
"__se_sys_rt_sigaction",
"__se_sys_rt_sigprocmask",
"__se_sys_rt_sigpending",
"__se_sys_rt_sigtimedwait",
"__se_sys_rt_sigtimedwait_time32",
"__se_sys_rt_sigqueueinfo",
"__se_sys_setpriority",
"__se_sys_getpriority",
"__se_sys_reboot",
"__se_sys_setregid",
"__se_sys_setgid",
"__se_sys_setreuid",
"__se_sys_setuid",
"__se_sys_setresuid",
"__se_sys_getresuid",
"__se_sys_setresgid",
"__se_sys_getresgid",
"__se_sys_setfsuid",
"__se_sys_setfsgid",
"__se_sys_times",
"__se_sys_setpgid",
"__se_sys_getpgid",
"__se_sys_getsid",
"__se_sys_setsid",
"__se_sys_getgroups",
"__se_sys_setgroups",
"__se_sys_newuname",
"__se_sys_sethostname",
"__se_sys_setdomainname",
"__se_sys_getrlimit",
"__se_sys_setrlimit",
"__se_sys_getrusage",
"__se_sys_umask",
"__se_sys_prctl",
"__se_sys_getcpu",
"__se_sys_gettimeofday",
"__se_sys_settimeofday",
"__se_sys_adjtimex",
"__se_sys_adjtimex_time32",
"__se_sys_getpid",
"__se_sys_getppid",
"__se_sys_getuid",
"__se_sys_geteuid",
"__se_sys_getgid",
"__se_sys_getegid",
"__se_sys_gettid",
"__se_sys_sysinfo",
"__se_sys_mq_open",
"__se_sys_mq_unlink",
"__se_sys_mq_timedsend",
"__se_sys_mq_timedreceive",
"__se_sys_mq_notify",
"__se_sys_mq_getsetattr",
"__se_sys_mq_timedreceive_time32",
"__se_sys_mq_timedsend_time32",
"__se_sys_msgget",
"__se_sys_old_msgctl",
"__se_sys_msgctl",
"__se_sys_msgrcv",
"__se_sys_msgsnd",
"__se_sys_semget",
"__se_sys_semctl",
"__se_sys_old_semctl",
"__se_sys_semtimedop",
"__se_sys_semtimedop_time32",
"__se_sys_semop",
"__se_sys_shmget",
"__se_sys_old_shmctl",
"__se_sys_shmctl",
"__se_sys_shmat",
"__se_sys_shmdt",
"__se_sys_socket",
"__se_sys_socketpair",
"__se_sys_bind",
"__se_sys_listen",
"__se_sys_accept",
"__se_sys_connect",
"__se_sys_getsockname",
"__se_sys_getpeername",
"__se_sys_sendto",
"__se_sys_recvfrom",
"__se_sys_setsockopt",
"__se_sys_getsockopt",
"__se_sys_shutdown",
"__se_sys_sendmsg",
"__se_sys_recvmsg",
"__se_sys_readahead",
"__se_sys_brk",
"__se_sys_munmap",
"__se_sys_mremap",
"__se_sys_add_key",
"__se_sys_request_key",
"__se_sys_keyctl",
"__se_sys_clone",
"__se_sys_clone3",
"__se_sys_execve",
"__se_sys_fadvise64_64",
"__se_sys_swapon",
"__se_sys_swapoff",
"__se_sys_mprotect",
"__se_sys_msync",
"__se_sys_mlock",
"__se_sys_munlock",
"__se_sys_mlockall",
"__se_sys_munlockall",
"__se_sys_mincore",
"__se_sys_madvise",
"__se_sys_remap_file_pages",
"__se_sys_mbind",
"__se_sys_get_mempolicy",
"__se_sys_set_mempolicy",
"__se_sys_migrate_pages",
"__se_sys_move_pages",
"__se_sys_rt_tgsigqueueinfo",
"__se_sys_perf_event_open",
"__se_sys_accept4",
"__se_sys_recvmmsg",
"__se_sys_recvmmsg_time32",
"__se_sys_wait4",
"__se_sys_prlimit64",
"__se_sys_fanotify_init",
"__se_sys_fanotify_mark",
"__se_sys_name_to_handle_at",
"__se_sys_open_by_handle_at",
"__se_sys_clock_adjtime",
"__se_sys_clock_adjtime32",
"__se_sys_syncfs",
"__se_sys_setns",
"__se_sys_pidfd_open",
"__se_sys_sendmmsg",
"__se_sys_process_vm_readv",
"__se_sys_process_vm_writev",
"__se_sys_kcmp",
"__se_sys_finit_module",
"__se_sys_sched_setattr",
"__se_sys_sched_getattr",
"__se_sys_renameat2",
"__se_sys_seccomp",
"__se_sys_getrandom",
"__se_sys_memfd_create",
"__se_sys_bpf",
"__se_sys_execveat",
"__se_sys_userfaultfd",
"__se_sys_membarrier",
"__se_sys_mlock2",
"__se_sys_copy_file_range",
"__se_sys_preadv2",
"__se_sys_pwritev2",
"__se_sys_pkey_mprotect",
"__se_sys_pkey_alloc",
"__se_sys_pkey_free",
"__se_sys_statx",
"__se_sys_rseq",
"__se_sys_open_tree",
"__se_sys_move_mount",
"__se_sys_fsopen",
"__se_sys_fsconfig",
"__se_sys_fsmount",
"__se_sys_fspick",
"__se_sys_pidfd_send_signal",
"__se_sys_pidfd_getfd",
"__se_sys_ioperm",
"__se_sys_pciconfig_read",
"__se_sys_pciconfig_write",
"__se_sys_pciconfig_iobase",
"__se_sys_spu_run",
"__se_sys_spu_create",
"__se_sys_open",
"__se_sys_link",
"__se_sys_unlink",
"__se_sys_mknod",
"__se_sys_chmod",
"__se_sys_chown",
"__se_sys_mkdir",
"__se_sys_rmdir",
"__se_sys_lchown",
"__se_sys_access",
"__se_sys_rename",
"__se_sys_symlink",
"__se_sys_stat64",
"__se_sys_lstat64",
"__se_sys_pipe",
"__se_sys_dup2",
"__se_sys_epoll_create",
"__se_sys_inotify_init",
"__se_sys_eventfd",
"__se_sys_signalfd",
"__se_sys_sendfile",
"__se_sys_newstat",
"__se_sys_newlstat",
"__se_sys_fadvise64",
"__se_sys_alarm",
"__se_sys_getpgrp",
"__se_sys_pause",
"__se_sys_time",
"__se_sys_time32",
"__se_sys_utime",
"__se_sys_utimes",
"__se_sys_futimesat",
"__se_sys_futimesat_time32",
"__se_sys_utime32",
"__se_sys_utimes_time32",
"__se_sys_creat",
"__se_sys_getdents",
"__se_sys_select",
"__se_sys_poll",
"__se_sys_epoll_wait",
"__se_sys_ustat",
"__se_sys_vfork",
"__se_sys_recv",
"__se_sys_send",
"__se_sys_bdflush",
"__se_sys_oldumount",
"__se_sys_uselib",
"__se_sys_sysctl",
"__se_sys_sysfs",
"__se_sys_fork",
"__se_sys_stime",
"__se_sys_stime32",
"__se_sys_sigpending",
"__se_sys_sigprocmask",
"__se_sys_sigsuspend",
"__se_sys_sigaction",
"__se_sys_sgetmask",
"__se_sys_ssetmask",
"__se_sys_signal",
"__se_sys_nice",
"__se_sys_kexec_file_load",
"__se_sys_waitpid",
"__se_sys_chown16",
"__se_sys_lchown16",
"__se_sys_fchown16",
"__se_sys_setregid16",
"__se_sys_setgid16",
"__se_sys_setreuid16",
"__se_sys_setuid16",
"__se_sys_setresuid16",
"__se_sys_getresuid16",
"__se_sys_setresgid16",
"__se_sys_getresgid16",
"__se_sys_setfsuid16",
"__se_sys_setfsgid16",
"__se_sys_getgroups16",
"__se_sys_setgroups16",
"__se_sys_getuid16",
"__se_sys_geteuid16",
"__se_sys_getgid16",
"__se_sys_getegid16",
"__se_sys_socketcall",
"__se_sys_stat",
"__se_sys_lstat",
"__se_sys_fstat",
"__se_sys_readlink",
"__se_sys_old_select",
"__se_sys_old_readdir",
"__se_sys_gethostname",
"__se_sys_uname",
"__se_sys_olduname",
"__se_sys_old_getrlimit",
"__se_sys_ipc",
"__se_sys_mmap_pgoff",
"__se_sys_old_mmap",
"__se_sys_ni_syscall",
};
    for (auto F : SyscallFN) {
        syscallFuncs.insert(F);
    }
}
#ifdef FindAlloc
llvm::Value *FindAlloc(llvm::Value *V)
{
	if (llvm::Instruction *I = dyn_cast<Instruction>(V))
	{
		switch(I->getOpCode())
		{
			case Instruction::Alloca:
			case Instruction::Add:
       			case Instruction::FAdd:
       			case Instruction::Sub:
       			case Instruction::FSub:
       			 case Instruction::Mul:
       			 case Instruction::FMul:
       			 case Instruction::SDiv:
       			 case Instruction::UDiv:
       			 case Instruction::FDiv:
       			 case Instruction::SRem:
       			 case Instruction::URem:
        case Instruction::And:
        case Instruction::Or:
        case Instruction::Xor:
        case Instruction::LShr:
        case Instruction::AShr:
        case Instruction::Shl:
			{
				return I;
			}
			case Instruction::Load:
			case Instruction::SExt:
       			case Instruction::ZExt:
			case Instruction::Trunc:	
			case Instruction::IntToPtr:
			case Instruction::PtrToInt:
			case Instruction::Select:
			{
				return FindAlloc(I->getOperand(0));
			}
			case Instruction::GetElementPtr:
			{
				return FindAlloc(I->getOperand(0));
			}
			
	
		}//wsitch
	}
}
#endif



#endif //UBIANALYSIS_CONFIG_H
