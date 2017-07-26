#ifndef __VPMU_ELF_H_
#define __VPMU_ELF_H_
#pragma once

#include <elf.h> // ELF header

bool is_ELF(void *eh_ptr);
bool read_elf64_header(int32_t fd, Elf64_Ehdr *elf_header);
bool read_elf32_header(int32_t fd, Elf32_Ehdr *elf_header);
Elf64_Phdr *read_elf64_program_header(int32_t fd, const Elf64_Ehdr elf_header);
Elf32_Phdr *read_elf32_program_header(int32_t fd, const Elf32_Ehdr elf_header);
bool is_elf64_dynamic(Elf64_Ehdr elf_header, Elf64_Phdr *program_header);
bool is_elf32_dynamic(Elf32_Ehdr elf_header, Elf32_Phdr *program_header);
int get_elf_word_size(int fd);

bool is_dynamic_binary(const char *file_path);

#endif
