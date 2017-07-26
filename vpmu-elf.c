#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // strncmp()
#include <stdbool.h> // bool
#include <unistd.h>  // lseek()
#include <fcntl.h>   // open()

#include "vpmu-elf.h" // Main header

bool is_ELF(void *eh_ptr)
{
    Elf64_Ehdr *eh = eh_ptr;
    /* ELF magic bytes are 0x7f,'E','L','F'
     * Using  octal escape sequence to represent 0x7f
     */
    if (!strncmp((char *)eh->e_ident, "\177ELF", 4)) {
        return 1;
    } else {
        return 0;
    }
}

bool read_elf64_header(int32_t fd, Elf64_Ehdr *elf_header)
{
    if (elf_header == NULL) return false;
    if (lseek(fd, (off_t)0, SEEK_SET) != (off_t)0) return false;
    if (read(fd, (void *)elf_header, sizeof(Elf64_Ehdr)) != sizeof(Elf64_Ehdr))
        return false;
    return true;
}

bool read_elf32_header(int32_t fd, Elf32_Ehdr *elf_header)
{
    if (elf_header == NULL) return false;
    if (lseek(fd, (off_t)0, SEEK_SET) != (off_t)0) return false;
    if (read(fd, (void *)elf_header, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr))
        return false;
    return true;
}

Elf64_Phdr *read_elf64_program_header(int32_t fd, const Elf64_Ehdr elf_header)
{
    const uint64_t buff_size = elf_header.e_phnum * sizeof(Elf64_Phdr);
    if (lseek(fd, (off_t)elf_header.e_phoff, SEEK_SET) != (off_t)elf_header.e_phoff)
        return NULL;
    Elf64_Phdr *program_header = (Elf64_Phdr *)malloc(buff_size);
    if (program_header == NULL) return NULL;
    if (read(fd, (void *)program_header, buff_size) != buff_size) {
        free(program_header);
        return NULL;
    }
    return program_header;
}

Elf32_Phdr *read_elf32_program_header(int32_t fd, const Elf32_Ehdr elf_header)
{
    const uint64_t buff_size = elf_header.e_phnum * sizeof(Elf32_Phdr);
    if (lseek(fd, (off_t)elf_header.e_phoff, SEEK_SET) != (off_t)elf_header.e_phoff)
        return NULL;
    Elf32_Phdr *program_header = (Elf32_Phdr *)malloc(buff_size);
    if (program_header == NULL) return NULL;
    if (read(fd, (void *)program_header, buff_size) != buff_size) {
        free(program_header);
        return NULL;
    }
    return program_header;
}

bool is_elf64_dynamic(Elf64_Ehdr elf_header, Elf64_Phdr *program_header)
{
    bool dynamic_flag = false;
    bool interp_flag  = false;
    // Iterative variable
    int i;

    if (program_header == NULL) return false;
    for (i = 0; i < elf_header.e_phnum; i++) {
        if (program_header[i].p_type == PT_INTERP) dynamic_flag = true;
        if (program_header[i].p_type == PT_DYNAMIC) interp_flag = true;
    }
    return interp_flag && dynamic_flag;
}

bool is_elf32_dynamic(Elf32_Ehdr elf_header, Elf32_Phdr *program_header)
{
    bool dynamic_flag = false;
    bool interp_flag  = false;
    // Iterative variable
    int i;

    if (program_header == NULL) return false;
    for (i = 0; i < elf_header.e_phnum; i++) {
        if (program_header[i].p_type == PT_INTERP) dynamic_flag = true;
        if (program_header[i].p_type == PT_DYNAMIC) interp_flag = true;
    }
    return interp_flag && dynamic_flag;
}

int get_elf_word_size(int fd)
{
    Elf64_Ehdr eh; // elf-header is fixed size

    // Try read header in 64 bits first
    read_elf64_header(fd, &eh);
    // Check magic words, return 0 when fail
    if (!is_ELF(&eh)) return 0;

    if (eh.e_ident[EI_CLASS] == 1) return 32;
    if (eh.e_ident[EI_CLASS] == 2) return 64;
    return 0;
}

bool is_dynamic_binary(const char *file_path)
{
    if (file_path == NULL) return false;
    int fd = open(file_path, O_RDONLY | O_SYNC);
    if (fd < 0) return false;

    // Check magic words and get ELF class size
    int ws = get_elf_word_size(fd);
    if (ws == 32) {
        Elf32_Ehdr  eh;   // elf-header is fixed size
        Elf32_Phdr *phdr; // program-header is variable size

        if (!read_elf32_header(fd, &eh)) return false;
        phdr = read_elf32_program_header(fd, eh);
        return is_elf32_dynamic(eh, phdr);
    } else if (ws == 64) {
        Elf64_Ehdr  eh;   // elf-header is fixed size
        Elf64_Phdr *phdr; // program-header is variable size

        if (!read_elf64_header(fd, &eh)) return false;
        phdr = read_elf64_program_header(fd, eh);
        return is_elf64_dynamic(eh, phdr);
    }

    return false;
}
