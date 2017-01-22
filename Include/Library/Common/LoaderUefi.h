/*
 * dmazar: UEFI wrapper for Mach-O definitions
 * xnu/EXTERNAL_HEADERS/mach-o/loader.h
 *
 */

#ifndef _UEFI_MACHO_LOADER_H_
#define _UEFI_MACHO_LOADER_H_

//
// Base UEFI types
//
#include <Include/Base.h>

typedef UINT8     uint8_t;
typedef UINT16    uint16_t;
typedef UINT32    uint32_t;
typedef UINT64    uint64_t;

typedef INT32     cpu_type_t;
typedef INT32     cpu_subtype_t;

typedef INT32     vm_prot_t;

struct mach_header_64 {
  UINT32          magic;           /* mach magic number identifier */
  cpu_type_t      cputype;          /* cpu specifier */
  cpu_subtype_t   cpusubtype;       /* machine specifier */
  UINT32          filetype;         /* type of file */
  UINT32          ncmds;            /* number of load commands */
  UINT32          sizeofcmds;       /* the size of all the load commands */
  UINT32          flags;            /* flags */
  UINT32          reserved;         /* reserved */
};

/* Constant for the magic field of the mach_header_64 (64-bit architectures) */
#define MH_MAGIC_64     0xfeedfacf  /* the 64-bit mach magic number */
#define MH_CIGAM_64     0xcffaedfe  /* NXSwapInt(MH_MAGIC_64) */
//#define LC_SEGMENT      0x1         /* segment of this file to be mapped */
#define LC_SYMTAB       0x2         /* link-edit stab symbol table info */
#define LC_UNIXTHREAD   0x5         /* unix thread (includes a stack) */
#define LC_SEGMENT_64   0x19        /* 64-bit segment of this file to be mapped */

struct segment_command_64 {         /* for 64-bit architectures */
  UINT32      cmd;                  /* LC_SEGMENT_64 */
  UINT32      cmdsize;              /* includes sizeof section_64 structs */
  CHAR8       segname[16];          /* segment name */
  UINT64      vmaddr;               /* memory address of this segment */
  UINT64      vmsize;               /* memory size of this segment */
  UINT64      fileoff;              /* file offset of this segment */
  UINT64      filesize;             /* amount to map from the file */
  vm_prot_t   maxprot;              /* maximum VM protection */
  vm_prot_t   initprot;             /* initial VM protection */
  UINT32      nsects;               /* number of sections in segment */
  UINT32      flags;                /* flags */
};

struct section_64 {                 /* for 64-bit architectures */
  CHAR8   sectname[16];             /* name of this section */
  CHAR8   segname[16];              /* segment this section goes in */
  UINT64  addr;                     /* memory address of this section */
  UINT64  size;                     /* size in bytes of this section */
  UINT32  offset;                   /* file offset of this section */
  UINT32  align;                    /* section alignment (power of 2) */
  UINT32  reloff;                   /* file offset of relocation entries */
  UINT32  nreloc;                   /* number of relocation entries */
  UINT32  flags;                    /* flags (section type and attributes)*/
  UINT32  reserved1;                /* reserved (for offset or index) */
  UINT32  reserved2;                /* reserved (for count or sizeof) */
  UINT32  reserved3;                /* reserved */
};

struct load_command {
  UINT32 cmd;                       /* type of load command */
  UINT32 cmdsize;                   /* total size of command in bytes */
};

/*
 * This is the symbol table entry structure for 64-bit architectures.
 */
struct nlist_64 {
    union {
      uint32_t  n_strx;   /* index into the string table */
    } n_un;

    uint8_t n_type;       /* type flag, see below */
    uint8_t n_sect;       /* section number or NO_SECT */
    uint16_t n_desc;      /* see <mach-o/stab.h> */
    uint64_t n_value;     /* value of this symbol (or stab offset) */
};

/*
 * The symtab_command contains the offsets and sizes of the link-edit 4.3BSD
 * "stab" style symbol table information as described in the header files
 * <nlist.h> and <stab.h>.
 */
struct symtab_command {
  uint32_t  cmd;      /* LC_SYMTAB */
  uint32_t  cmdsize;  /* sizeof (struct symtab_command) */
  uint32_t  symoff;   /* symbol table offset */
  uint32_t  nsyms;    /* number of symbol table entries */
  uint32_t  stroff;   /* string table offset */
  uint32_t  strsize;  /* string table size in bytes */
};

//
// From xnu/osfmk/mach/i386/_structs.h:
//

#define _STRUCT_X86_THREAD_STATE32  struct __darwin_i386_thread_state
_STRUCT_X86_THREAD_STATE32
{
    // all fields are unsigned int in xnu rources
  UINT32  eax;
  UINT32  ebx;
  UINT32  ecx;
  UINT32  edx;
  UINT32  edi;
  UINT32  esi;
  UINT32  ebp;
  UINT32  esp;
  UINT32  ss;
  UINT32  eflags;
  UINT32  eip;
  UINT32  cs;
  UINT32  ds;
  UINT32  es;
  UINT32  fs;
  UINT32  gs;
};

#define _STRUCT_X86_THREAD_STATE64  struct __darwin_x86_thread_state64
_STRUCT_X86_THREAD_STATE64
{
  UINT64  rax;
  UINT64  rbx;
  UINT64  rcx;
  UINT64  rdx;
  UINT64  rdi;
  UINT64  rsi;
  UINT64  rbp;
  UINT64  rsp;
  UINT64  r8;
  UINT64  r9;
  UINT64  r10;
  UINT64  r11;
  UINT64  r12;
  UINT64  r13;
  UINT64  r14;
  UINT64  r15;
  UINT64  rip;
  UINT64  rflags;
  UINT64  cs;
  UINT64  fs;
  UINT64  gs;
};

//
// From xnu/osfmk/mach/i386/thread_status.h:
//
typedef _STRUCT_X86_THREAD_STATE32 i386_thread_state_t;
typedef _STRUCT_X86_THREAD_STATE64 x86_thread_state64_t;

#endif /* _UEFI_MACHO_LOADER_H_ */
