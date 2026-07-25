#pragma once

#include "scheduler/scheduler.h"
#include <stdint.h>

#define EI_NIDENT 16
#define SHN_UNDEF 0


typedef struct {
    uint8_t   e_ident[EI_NIDENT];
    uint16_t  e_type;
    uint16_t  e_machine;
    uint32_t  e_version;
    uint64_t  e_entry;
    uint64_t  e_phoff;
    uint64_t  e_shoff;
    uint32_t  e_flags;
    uint16_t  e_ehsize;
    uint16_t  e_phentsize;
    uint16_t  e_phnum;
    uint16_t  e_shentsize;
    uint16_t  e_shnum;
    uint16_t  e_shstrndx;
} elf64_ehdr;


enum Elf_Ident {
    EI_MAG0        = 0, // 0x7F
    EI_MAG1        = 1, // 'E'
    EI_MAG2        = 2, // 'L'
    EI_MAG3        = 3, // 'F'
    EI_CLASS       = 4, // Architecture (32/64)
    EI_DATA        = 5, // Byte Order
    EI_VERSION     = 6, // ELF Version
    EI_OSABI       = 7, // OS Specific
    EI_ABIVERSION  = 8, // OS Specific
    EI_PAD         = 9  // Padding
};

#define ELFMAG0     0x7F // e_ident[EI_MAG0]
#define ELFMAG1     'E'  // e_ident[EI_MAG1]
#define ELFMAG2     'L'  // e_ident[EI_MAG2]
#define ELFMAG3     'F'  // e_ident[EI_MAG3]

#define ELFDATA2LSB 1    // Little Endian
#define ELFCLASS64  2    // 64-bit Architecture

#define EM_X86_64   62
#define EV_CURRENT  1    // ELF Current Version

typedef struct {
    uint32_t p_type;    // Segment type
    uint32_t p_flags;   // Flags (R/W/X) -- note: comes before offset in Elf64
    uint64_t p_offset;  // File offset
    uint64_t p_vaddr;   // Virtual address
    uint64_t p_paddr;   // Physical address
    uint64_t p_filesz;  // Size in file
    uint64_t p_memsz;   // Size in memory
    uint64_t p_align;   // Alignment
} elf64_phdr;

typedef enum {
    PT_NULL    = 0,
    PT_LOAD    = 1,
    PT_DYNAMIC = 2,
    PT_INTERP  = 3,
    PT_NOTE    = 4,
    PT_SHLIB   = 5,
    PT_PHDR    = 6,
    PT_TLS     = 7
} elf64_segment_type;


typedef enum {
    PF_X = 1 << 0,
    PF_W = 1 << 1,
    PF_R = 1 << 2
} elf64_segment_flags;


typedef enum {
    ET_NONE = 0, // No file type
    ET_REL  = 1, // Relocatable
    ET_EXEC = 2, // Executable
    ET_DYN  = 3, // Shared object
    ET_CORE = 4  // Core file
} elf64_file_type;


typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} elf64_shdr;


typedef enum {
    SHT_NULL     = 0,
    SHT_PROGBITS = 1,
    SHT_SYMTAB   = 2,
    SHT_STRTAB   = 3,
    SHT_RELA     = 4,
    SHT_HASH     = 5,
    SHT_DYNAMIC  = 6,
    SHT_NOTE     = 7,
    SHT_NOBITS   = 8,
    SHT_REL      = 9
} elf64_section_type;


typedef struct {
    uint32_t st_name;   // Index into string table
    uint8_t  st_info;   // Type and binding
    uint8_t  st_other;  // Visibility
    uint16_t st_shndx;  // Section index
    uint64_t st_value;  // Value (address or offset)
    uint64_t st_size;   // Size in bytes
} elf64_sym;


typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
} elf64_rel;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} elf64_rela;


#define ELF64_R_SYM(info)  ((info) >> 32)
#define ELF64_R_TYPE(info) ((uint32_t)(info))


typedef enum {
    R_X86_64_NONE     = 0,
    R_X86_64_64       = 1,  // S + A
    R_X86_64_PC32     = 2,  // S + A - P
    R_X86_64_GLOB_DAT = 6,
    R_X86_64_JUMP_SLOT= 7,
    R_X86_64_RELATIVE = 8   // B + A
} elf64_relocation_type;


int elf64_validate(elf64_ehdr* eh);
pid_t load_elf_from_vfs(const char* vfs_path, char** argv, char** envp);