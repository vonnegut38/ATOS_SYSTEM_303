/* Shell filesystem builtins: COPY / MOVE / DEL / MKDIR.
 *
 * Kept out of shell_module.S on purpose (docs/SHELL-REFACTOR-PLAN.md): the
 * assembly dispatch passes the raw line, the current directory and the kernel
 * module API; this file owns path handling and console output.  shell_fs_run()
 * returns 1 when the line named one of these commands (handled, success or
 * error), 0 when it is some other command and the shell must keep dispatching.
 *
 * The kernel FAT layer already implements the mutations; the v6 module API
 * tail exposes them as copy_path/mkdir_path/delete_path/move_path. */
#include <stdint.h>
#include "kernel_module.h"
#include "tos_ext.h"

int shell_build_path(const char *current,const char *argument,char *output,
                     uint32_t capacity);

#define SHELL_FS_ARGUMENT 80

enum {
    SHELL_FS_COPY = 0, SHELL_FS_MOVE = 1, SHELL_FS_DELETE = 2, SHELL_FS_MKDIR = 3
};

/* The names live inline in the table, not as pointers: the shell module is
   loaded at a runtime address (kernel_module.c: entry = image + offset), so a
   pointer stored in static data would still hold the link-time address. */
static const struct {
    char name[8];
    uint8_t operation,arguments;
} shell_fs_commands[] = {
    {"copy", SHELL_FS_COPY, 2u},   {"cp", SHELL_FS_COPY, 2u},
    {"move", SHELL_FS_MOVE, 2u},   {"ren", SHELL_FS_MOVE, 2u},
    {"rename", SHELL_FS_MOVE, 2u},
    {"del", SHELL_FS_DELETE, 1u},  {"delete", SHELL_FS_DELETE, 1u},
    {"erase", SHELL_FS_DELETE, 1u},{"rm", SHELL_FS_DELETE, 1u},
    {"mkdir", SHELL_FS_MKDIR, 1u}, {"md", SHELL_FS_MKDIR, 1u},
};

static int shell_fs_equal(const char *text,const char *command)
{
    while(*command)if(*text++!=*command++)return 0;
    return *text=='\0';
}

static const char *shell_fs_skip(const char *text)
{
    while(*text==' '||*text=='\t')++text;
    return text;
}

/* Copy the next whitespace-delimited token.  1 = token, 0 = end of line,
   -1 = token longer than the destination. */
static int shell_fs_token(const char **cursor,char *output,uint32_t capacity)
{
    const char *text=shell_fs_skip(*cursor);uint32_t used=0;
    if(!*text){*cursor=text;return 0;}
    while(*text&&*text!=' '&&*text!='\t'){
        if(used+1u>=capacity)return -1;
        output[used++]=*text++;
    }
    output[used]='\0';*cursor=text;return 1;
}

static void shell_fs_error(const kernel_module_api_t *api,int status)
{
    switch(-status){
    case FAT_WRITE_IO:        api->console_puts("I/O ERROR\n");break;
    case FAT_WRITE_INVALID:   api->console_puts("INVALID PATH\n");break;
    case FAT_WRITE_EXISTS:    api->console_puts("ALREADY EXISTS\n");break;
    case FAT_WRITE_FULL:      api->console_puts("DISK FULL\n");break;
    case FAT_WRITE_NOT_EMPTY: api->console_puts("DIRECTORY NOT EMPTY\n");break;
    case FAT_WRITE_READ_ONLY: api->console_puts("READ ONLY\n");break;
    case FAT_WRITE_LIMIT:     api->console_puts("FILE TOO BIG\n");break;
    default:                  api->console_puts("FAILED\n");break;
    }
}

static void shell_fs_usage(const kernel_module_api_t *api,uint8_t operation)
{
    const char *text;
    if(operation==SHELL_FS_COPY)text="USAGE: COPY <SOURCE> <TARGET>\n";
    else if(operation==SHELL_FS_MOVE)text="USAGE: MOVE <SOURCE> <TARGET>\n";
    else if(operation==SHELL_FS_DELETE)text="USAGE: DEL <PATH>\n";
    else text="USAGE: MKDIR <PATH>\n";
    api->console_puts(text);
}

int shell_fs_run(const char *line,const char *current,
                 const kernel_module_api_t *api)
{
    char command[12],source_arg[SHELL_FS_ARGUMENT],target_arg[SHELL_FS_ARGUMENT];
    char source[SHELL_FS_ARGUMENT],target[SHELL_FS_ARGUMENT];
    const char *cursor;uint8_t operation=0,arguments=0;unsigned i;int got,status;
    if(!line||!current||!api)return 0;
    /* Older kernels stop at tos_run; do not read the v6 tail on those. */
    if(api->size<sizeof(kernel_module_api_t))return 0;
    cursor=line;
    if(shell_fs_token(&cursor,command,sizeof(command))!=1)return 0;
    for(i=0;i<sizeof(shell_fs_commands)/sizeof(shell_fs_commands[0]);++i)
        if(shell_fs_equal(command,shell_fs_commands[i].name)){
            operation=shell_fs_commands[i].operation;
            arguments=shell_fs_commands[i].arguments;break;}
    if(i==sizeof(shell_fs_commands)/sizeof(shell_fs_commands[0]))return 0;
    got=shell_fs_token(&cursor,source_arg,sizeof(source_arg));
    if(got!=1){shell_fs_usage(api,operation);return 1;}
    if(!shell_build_path(current,source_arg,source,sizeof(source))){
        api->console_puts("PATH TOO LONG\n");return 1;}
    if(arguments==2u){
        got=shell_fs_token(&cursor,target_arg,sizeof(target_arg));
        if(got!=1){shell_fs_usage(api,operation);return 1;}
        if(!shell_build_path(current,target_arg,target,sizeof(target))){
            api->console_puts("PATH TOO LONG\n");return 1;}
    }
    if(operation==SHELL_FS_COPY)status=api->copy_path(source,target);
    else if(operation==SHELL_FS_MOVE)status=api->move_path(source,target);
    else if(operation==SHELL_FS_DELETE)status=api->delete_path(source);
    else status=api->mkdir_path(source);
    if(status<0)shell_fs_error(api,status);
    return 1;
}

/* Report a TOS program's exit code on the shell console.  The sign and the
 * magnitude are emitted separately through console_put_u32(), so no divide or
 * modulo helper (libgcc __udivsi3/__umodsi3) is pulled into the -mpcrel shell
 * module.  0u-magnitude is well-defined even for INT32_MIN. */
static void shell_tos_report(const kernel_module_api_t *api,int32_t rc)
{
    uint32_t magnitude=(uint32_t)rc;
    api->console_puts("TOS RC ");
    if(rc<0){
        api->console_puts("-");
        magnitude=0u-magnitude;
    }
    api->console_put_u32(magnitude);
    api->console_puts("\n");
}

/* Shared launch decision for the shell (the "commands" launch point): a
 * .PRG/.TOS/.TTP/.GTP name runs through the TOS personality instead of the
 * native ASYSAPP0 spawn path.  Returns 1 when the program was run (success or
 * fault), 0 when the path is not a TOS program and the caller must keep its
 * normal native spawn. */
int shell_tos_dispatch(const char *path,const char *arguments,
                       const kernel_module_api_t *api)
{
    int32_t rc;
    if(!path||!api)return 0;
    /* tos_run is the v5 tail at offset 188; a kernel without it must fall
       through to the native path, never call through a missing slot. */
    if(api->size<192u)return 0;
    if(!tos_program_path_ext(path))return 0;
    /* Arguments flow through the same set_user_arguments() the native spawn
       uses; kernel_tos_run_program() copies them into the basepage command
       line.  Reset afterwards so a later native run does not inherit them. */
    api->set_user_arguments(arguments?arguments:"");
    rc=api->tos_run(path);
    api->set_user_arguments("");
    shell_tos_report(api,rc);
    return 1;
}
