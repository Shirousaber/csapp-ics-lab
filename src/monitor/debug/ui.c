#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static void dum_regs(){
  int i;
  for(i=R_EAX; i<=R_EDI; i++){
    printf("%s:0x%08x\n",regsl[i],cpu.gpr[i]._32);
  }
  printf("eip :0x%08x\n",cpu.eip);
}

static int cmd_si(char *args) {
  if(args!=NULL){
   int temp_arg = atoi(args);
   cpu_exec(temp_arg);
   return 0;
  } 
  cpu_exec(1);
  return 0;
}

static int cmd_x(char *args){
  int addr, len;
  sscanf(args,"%d 0x%x",&len,&addr);
  int i;
  for(i=1;i<=len;i++)
  {
    printf("0x%x ",addr);
    int temp = vaddr_read(addr, 4);
    printf("0x%x\n",temp);
    addr+=4;
  }
  return 0;
}

static int cmd_info(char *args){
  switch(*args){
   case 'r':dum_regs();return 0;
   default: return 1;
  }
}

static int cmd_p(char *args)
{
  char *arg = strtok(NULL,"@");//this is to get all the params
  bool success = true;
  int result = expr(arg, &success);
  if(!success)
    printf("match rules fails!\n");
  else
    printf("%#x\n", result);//%#x :0x(result in hex)
  return 0;
}
static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si","step-by step debug",cmd_si},
  { "info","registor detailed information", cmd_info},
  {"x","scanf the memory", cmd_x},
  {"p", "get the express answer", cmd_p}
  /* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  while (1) {
    char *str = rl_gets();
    char *str_end = str+strlen(str);
    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue(void);
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}
