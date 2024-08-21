#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <string.h>

// 매크로 정의
#define SYM_TYPE(n_type) ((n_type) & N_TYPE)  // N_TYPE 매크로를 사용해 심볼 타입 추출
#define SYM_BIND(n_type) ((n_type) & N_EXT)   // N_EXT 매크로를 사용해 심볼 바인딩 추출
#define SYM_SECT(n_type) ((n_type) & N_SECT)  // N_SECT 매크로를 사용해 섹션 추출

typedef struct {
    uint64_t address;
    char type;
    char *name;
} Symbol;

int ft_strcmp(const char *str, const char *str2)
{
    if (str == NULL || str2 == NULL)
        return (1);
    return strcmp(str, str2);
}

void print_symbols(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct stat stat_buf;
    if (fstat(fd, &stat_buf) == -1) {
        perror("fstat");
        close(fd);
        exit(EXIT_FAILURE);
    }

    void *file = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    struct mach_header_64 *header = (struct mach_header_64 *)file;
    struct load_command *lc = (struct load_command *)(header + 1);
    Symbol *symbols = NULL;
    uint32_t sy_size = 0;
    struct symtab_command *symtab = NULL;
    struct segment_command_64 *seg = NULL;
    
    // Iterate through load commands to find the symbol table and segment commands
    for (uint32_t i = 0; i < header->ncmds; i++) {
        if (lc->cmd == LC_SYMTAB) {
            symtab = (struct symtab_command *)lc;
        } else if (lc->cmd == LC_SEGMENT_64) {
            seg = (struct segment_command_64 *)lc;
        }
        lc = (struct load_command *)((char *)lc + lc->cmdsize);
    }
 if (symtab) {
        char *strtab = (char *)(file + symtab->stroff);
        struct nlist_64 *symtab_entries = (struct nlist_64 *)(file + symtab->symoff);
        symbols = malloc(sizeof(Symbol) * symtab->nsyms);
        sy_size = symtab->nsyms;

        for (uint32_t j = 0; j < sy_size; j++) {
            uint8_t type = symtab_entries[j].n_type & N_TYPE;
            uint8_t sect = symtab_entries[j].n_sect;
            uint8_t ext = symtab_entries[j].n_type & N_EXT;

            if (type == N_SECT) {
                if (ext) {
                    if (sect == 1) { // __text section
                        symbols[j].type = 'T'; // 전역 함수
                    } else {
                        symbols[j].type = 'S'; // 전역 변수 (기본값)
                    }
                } else {
                    symbols[j].type = 't'; // 로컬 심볼
                }
            } else if (type == N_UNDF)
                symbols[j].type = 'U';
            else if (type == N_ABS)
                symbols[j].type = 'A';
            else if (type == N_PBUD)
                symbols[j].type = 'P';
            else if (type == N_INDR)
                symbols[j].type = 'I';
            else {
                symbols[j].type = '?'; // 기타 심볼 (필요에 따라 처리)
            }

            symbols[j].address = symtab_entries[j].n_value;
            symbols[j].name = &strtab[symtab_entries[j].n_un.n_strx];
        }

        // 섹션 정보를 이용한 추가 분류 (필요한 경우)
        if (seg) {
            struct section_64 *sections = (struct section_64 *)((char *)seg + sizeof(struct segment_command_64));
            for (uint32_t j = 0; j < sy_size; j++) {
                if (symbols[j].type == 'S' || symbols[j].type == 'T') {
                    uint8_t sect = symtab_entries[j].n_sect;
                    if (sect > 0 && sect <= seg->nsects) {
                        struct section_64 *section = &sections[sect - 1];
                        if (strcmp(section->sectname, "__text") == 0) {
                            symbols[j].type = 'T';
                        } else if (strcmp(section->sectname, "__data") == 0 || 
                                   strcmp(section->sectname, "__bss") == 0) {
                            symbols[j].type = 'S';
                        }
                    }
                }
            }
        }
    }

    // Sort symbols by name
    for (uint32_t i = 1; i < sy_size; i++) {
        for (uint32_t x = i; x > 0; x--) {
            if (ft_strcmp(symbols[x - 1].name, symbols[x].name) > 0) {
                Symbol symbol = symbols[x];
                symbols[x] = symbols[x - 1];
                symbols[x - 1] = symbol;
            }
        }
    }

    // Print symbols
    for (uint32_t i = 0; i < sy_size; i++) {
        if (symbols[i].address != 0)
            printf("%016llx %c %s\n", symbols[i].address, symbols[i].type, symbols[i].name);
        else
            printf("                 %c %s\n", symbols[i].type, symbols[i].name);
    }

    // Clean up
    munmap(file, stat_buf.st_size);
    close(fd);
    free(symbols);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <mach-o-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    print_symbols(argv[1]);
    return EXIT_SUCCESS;
}

//------------------------------------------------------------------------

// #include <stdio.h>
// #include <stdlib.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <sys/mman.h>
// #include <sys/stat.h>
// #include <mach-o/loader.h>
// #include <mach-o/nlist.h>

// // 매크로 정의
// #define SYM_TYPE(n_type) ((n_type) & N_TYPE)  // N_TYPE 매크로를 사용해 심볼 타입 추출
// #define SYM_BIND(n_type) ((n_type) & N_EXT)   // N_EXT 매크로를 사용해 심볼 바인딩 추출
// #define SYM_SECT(n_type) ((n_type) & N_SECT)  // N_SECT 매크로를 사용해 섹션 추출

// typedef struct {
//     uint64_t address;
//     char type;
//     char *name;
// } Symbol;

// int ft_strcmp(char *str, char *str2)
// {
//     int i;

//     if (str == NULL || str2 == NULL)
//         return (1);
//     i = 0;
//     while (str && str2 && str[i] && str2[i])
//     {
//         if (str[i] != str2[i])
//             break;
//         i++;
//     }
//     return (str[i] - str2[i]);
// }

// void print_symbols(const char *filename) {
//     int fd = open(filename, O_RDONLY);
//     if (fd == -1) {
//         perror("open");
//         exit(EXIT_FAILURE);
//     }

//     struct stat stat_buf;
//     if (fstat(fd, &stat_buf) == -1) {
//         perror("fstat");
//         close(fd);
//         exit(EXIT_FAILURE);
//     }

//     void *file = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
//     if (file == MAP_FAILED) {
//         perror("mmap");
//         close(fd);
//         exit(EXIT_FAILURE);
//     }

//     struct mach_header_64 *header = (struct mach_header_64 *)file;
//     struct load_command *lc = (struct load_command *)(header + 1);
//     Symbol *symbols;
//     uint32_t sy_size;
//     for (uint32_t i = 0; i < header->ncmds; i++) {
//         if (lc->cmd == LC_SYMTAB) {
//             struct symtab_command *symtab = (struct symtab_command *)lc;
//             char *strtab = (char *)(file + symtab->stroff);
//             struct nlist_64 *symtab_entries = (struct nlist_64 *)(file + symtab->symoff);
//             symbols = malloc(sizeof(Symbol) * (symtab->nsyms + 1));
//             sy_size = symtab->nsyms;
//             for (uint32_t j = 0; j < symtab->nsyms; j++) {
//                 char symbol_type;
//                 uint8_t type = SYM_TYPE(symtab_entries[j].n_type);
//                 uint8_t bind = SYM_BIND(symtab_entries[j].n_type);
//                 uint8_t sect = SYM_SECT(symtab_entries[j].n_type);
//                 // 심볼 타입을 판별하여 출력
//                 switch (type) {
//                     case N_UNDF:
//                         symbol_type = 'U'; // 정의되지 않음 (Undefined)
//                         break;
//                     case N_ABS:
//                         symbol_type = 'A'; // 절대 주소 (Absolute)
//                         break;
//                     case N_SECT:
//                        // 섹션에 위치하는 심볼
//                         if (bind == N_EXT) {
//                             symbol_type = 'T'; // 텍스트 섹션 (함수)
//                             if (symtab_entries[j].n_sect != 0) {
//                                 // 데이터 섹션에 위치하는 전역 변수일 경우
//                                 symbol_type = 'S'; // 전역 변수 (Static data)
//                             }
//                         } else {
//                             symbol_type = 't'; // 내부 바인딩은 변수로 간주
//                         }
//                         break;
//                     case N_PBUD:
//                         symbol_type = 'P'; // 프로그램 내부 정의 (Pre-Bound)
//                         break;
//                     case N_INDR:
//                         symbol_type = 'I'; // 간접 (Indirect)
//                         break;
//                     default:
//                         symbol_type = '?'; // 알려지지 않음
//                         break;
//                 }
//                 symbols[j].address = symtab_entries[j].n_value;
//                 symbols[j].type = symbol_type;
//                 symbols[j].name = &strtab[symtab_entries[j].n_un.n_strx];
//             }
//         }
//         lc = (struct load_command *)((char *)lc + lc->cmdsize);
//     }
//     for (uint32_t i = 1; i < sy_size; i++)
//     {
//         for (uint32_t x = i; x > 0; x--)
//         {
//             if (ft_strcmp(symbols[x - 1].name, symbols[x].name) > 0)
//             {
//                 Symbol symbol = symbols[x];
//                 symbols[x] = symbols[x - 1];
//                 symbols[x - 1] = symbol;
//             }
//         }
//     }
//     for (uint32_t i = 0; i < sy_size; i++)
//     {
//         if (symbols[i].address != 0)
//             printf("%016llx %c %s\n", symbols[i].address, symbols[i].type, symbols[i].name);
//         else
//             printf("                 %c %s\n", symbols[i].type, symbols[i].name);
//     }
//     munmap(file, stat_buf.st_size);
//     close(fd);
// }

// int main(int argc, char *argv[]) {
//     if (argc != 2) {
//         fprintf(stderr, "Usage: %s <mach-o-file>\n", argv[0]);
//         return EXIT_FAILURE;
//     }

//     print_symbols(argv[1]);
//     return EXIT_SUCCESS;
// }

//------------------------------------------------------------------------

// #include <stdio.h>
// #include <stdlib.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <sys/mman.h>
// #include <sys/stat.h>
// #include <mach-o/loader.h>
// #include <mach-o/nlist.h>

// void print_symbols(const char *filename) {
//     int fd = open(filename, O_RDONLY);
//     if (fd == -1) {
//         perror("open");
//         exit(EXIT_FAILURE);
//     }

//     struct stat stat_buf;
//     if (fstat(fd, &stat_buf) == -1) {
//         perror("fstat");
//         close(fd);
//         exit(EXIT_FAILURE);
//     }

//     void *file = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
//     if (file == MAP_FAILED) {
//         perror("mmap");
//         close(fd);
//         exit(EXIT_FAILURE);
//     }

//     struct mach_header_64 *header = (struct mach_header_64 *)file;
//     struct load_command *lc = (struct load_command *)(header + 1);

//     for (uint32_t i = 0; i < header->ncmds; i++) {
//         if (lc->cmd == LC_SYMTAB) {
//             struct symtab_command *symtab = (struct symtab_command *)lc;
//             char *strtab = (char *)(file + symtab->stroff);
//             struct nlist_64 *symtab_entries = (struct nlist_64 *)(file + symtab->symoff);

//             for (uint32_t j = 0; j < symtab->nsyms; j++) {
//                 if ((symtab_entries[j].n_type & N_TYPE) == N_UNDF) {
//                     continue;
//                 }
//                 printf("Symbol: %s\n", &strtab[symtab_entries[j].n_un.n_strx]);
//             }
//         }
//         lc = (struct load_command *)((char *)lc + lc->cmdsize);
//     }

//     munmap(file, stat_buf.st_size);
//     close(fd);
// }

// int main(int argc, char *argv[]) {
//     if (argc != 2) {
//         fprintf(stderr, "Usage: %s <mach-o-file>\n", argv[0]);
//         return EXIT_FAILURE;
//     }

//     print_symbols(argv[1]);
//     return EXIT_SUCCESS;
// }

//------------------------------------------------------------------------

// #include <unistd.h>
// #include <stdlib.h>
// #include <stdio.h>
// #include <sys/mman.h>
// #include <fcntl.h>
// #include <sys/types.h>
// #include <sys/stat.h>

// int main(int argc, char **argv)
// {
//     int fd = open(argv[1], O_RDONLY);
//      if (fd == -1) {
//         perror("open");
//         return EXIT_FAILURE;
//     }

//     struct stat file_stat;
//     if (fstat(fd, &file_stat) == -1) {
//         perror("fstat");
//         close(fd);
//         return EXIT_FAILURE;
//     }

//     off_t file_size = file_stat.st_size;

//     char *mapped = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
//     if (mapped == MAP_FAILED) {
//         perror("mmap");
//         close(fd);
//         return EXIT_FAILURE;
//     }

//     if (write(STDOUT_FILENO, mapped, file_size) == -1) {
//         perror("write");
//         munmap(mapped, file_size);
//         close(fd);
//         return EXIT_FAILURE;
//     }

//     if (munmap(mapped, file_size) == -1) {
//         perror("munmap");
//         close(fd);
//         return EXIT_FAILURE;
//     }

//     close(fd);
//     return EXIT_SUCCESS;
// }