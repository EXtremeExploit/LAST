#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <lua.h>
#include <stdio.h>

#include "process.h"
#include "auto-splitter.h"

struct last_process process;

void execute_command(const char* command, char* buffer, char* output)
{
    FILE* pipe = popen(command, "r");
    if (!pipe)
    {
        fprintf(stderr, "Error executing command: %s\n", command);
        exit(1);
    }

    while (fgets(buffer, 128, pipe) != NULL)
    {
        strcat(output, buffer);
    }

    pclose(pipe);
}

/*
Gets the base address of a module
if `module` equals to a nullptr, the main process is used, else it will search for the base addr of the specified module
*/
uintptr_t find_base_address(const char* module)
{
    const char* module_to_grep = module == 0 ? process.name : module;

    char path[22]; // 22 is the maximum length the path can be (strlen("/proc/4294967296/maps"))

    snprintf(path, sizeof(path), "/proc/%d/maps", process.pid);

    FILE *f = fopen(path, "r");

#ifdef __USE_GNU

    printf("aaaaaa")
#endif

    if (f) {
        struct timespec clock_start_find;
        clock_gettime(CLOCK_MONOTONIC, &clock_start_find);
        char current_line[1024];
        int fgetsCounter = 0;
        while (fgets_unlocked(current_line, sizeof(current_line), f) != NULL) {
            fgetsCounter++;
            if (strstr(current_line, module_to_grep) == NULL)
                continue;
            printf("fgetsCounter: %d ", fgetsCounter);
            struct timespec clock_end_find;
            clock_gettime(CLOCK_MONOTONIC, &clock_end_find);
            long long find_duration = (clock_end_find.tv_sec - clock_start_find.tv_sec) * 1000000 + (clock_end_find.tv_nsec - clock_start_find.tv_nsec) / 1000;
            printf("find: %llu\n", find_duration);
            fclose(f);
            size_t dash_pos = strcspn(current_line, "-");

            if (dash_pos != strlen(current_line)) {
                char first_number[32];
                strncpy(first_number, current_line, dash_pos);
                first_number[dash_pos] = '\0';
                uintptr_t addr = strtoull(first_number, NULL, 16);
                return addr;
                break;
            }
        }
        fclose(f);
    }
    printf("Couldn't find base address\n");
    return 0;
}

void stock_process_id(const char* processtarget)
{
    char pid_command[128];
    strcpy(pid_command, processtarget);

    char buffer[128];
    char pid_output[128];
    pid_output[0] = '\0';

    while (process.pid == 0 && atomic_load(&auto_splitter_enabled))
    {
        execute_command(pid_command, buffer, pid_output);
        size_t space_pos = strcspn(pid_output, " ");
        if (space_pos != strlen(pid_output))
        {
            printf("Multiple PID's found for process: %s\n", process.name);
        }
        process.pid = strtoul(pid_output, NULL, 10);
        printf("\033[2J\033[1;1H"); // Clear the console
        usleep(100000);
        printf("%s isn't running.\n", process.name);
    }
    
    if (process.pid != 0)
    {
        printf("\033[2J\033[1;1H"); // Clear the console
        printf("Process: %s\n", process.name);
        printf("PID: %u\n", process.pid);
        process.base_address = find_base_address(0);
        process.dll_address = process.base_address;
    }
}

int find_process_id(lua_State* L)
{
    process.name = lua_tostring(L, 1);
    char command[256];
    printf("\033[2J\033[1;1H"); // Clear the console
    snprintf(command, sizeof(command), "pgrep \"%.*s\"", (int)strnlen(process.name, 15), process.name);

    stock_process_id(command);

    return 0;
}

int process_exists()
{
    int result = kill(process.pid, 0);
    return result == 0;
}