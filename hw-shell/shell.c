#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print the current working directory"},
    {cmd_cd, "cd", "change the current directory"}
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

int cmd_pwd(struct tokens* tokens) {
  char* buffer = (char*) malloc(1024);
    if (getcwd(buffer, 1024) != NULL) {
      printf("%s\n", buffer);
      free(buffer);
      return 1;
    } 
  return -1;
}

/* Change current directory */
int cmd_cd(struct tokens* tokens) {
  if (tokens == NULL) {
    return -1;
  } else {
    if (chdir(tokens_get_token(tokens, 1)) == -1) {
      return -1;
    }
    return 1;
  }
}

/* Redirect files */
void* redirect_files(char* argv[]) {
  for (int i=1; argv[i] != NULL; i++) {
    if (strcmp(argv[i], "<") == 0) {
      int fd = open(argv[i+1], O_RDONLY | O_CREAT, 0640);
      dup2(fd, STDIN_FILENO);
      close(fd);
      //freopen(argv[i+1], "r", stdin);
      argv[i] = NULL;
    } else if (strcmp(argv[i], ">") == 0) {
      int fd = open(argv[i+1], O_WRONLY | O_CREAT, 0640);
      dup2(fd, STDOUT_FILENO);
      close(fd);
      //freopen(argv[i+1], "w", stdout);
      argv[i] = NULL;
    } 
  }
  return NULL;
}


/* Parsing path for path resolution */
void* execute_cmd(char* dir, char* argv[]) {


    /* path directories parsed by ":" */
    char* path_dirs = getenv("PATH");
    char* all_dirs = malloc(sizeof(char) * strlen(path_dirs));
    all_dirs = strcpy(all_dirs, path_dirs);
    char* token;
    char* rest = all_dirs;

    /* if user puts in full path */
    execv(dir, argv);
  
    while ((token = strtok_r(rest, ":", &rest)) != NULL) {
      char* valid_path = malloc(sizeof(char) * (strlen(dir) + strlen(token) + 1));
      strcat(valid_path, token);
      strcat(valid_path, "/");
      strcat(valid_path, dir);

      printf("%s", valid_path);
    
      execv(valid_path, argv);
     } 
     
     perror("exec failed");
     exit(-1);
  
  return NULL;
}

int get_num_pipes(struct tokens *tokens) {
  int num_pipes = 0;
  for (int i=0; i<tokens_get_length(tokens); i++) {
    if (strcmp(tokens_get_token(tokens, i), "|") == 0) {
      num_pipes += 1;
    }
  }
  return num_pipes;
}

/* separate into different commands dependent on "|" */
char*** parse_args(int num_pipes, struct tokens* tokens) {
  char*** commands;
  commands = malloc(sizeof(char**) * (num_pipes + 1));

  int save = 0;
  for (int i=0; i < num_pipes+1; i++) {
    commands[i] = malloc(sizeof(char*) * (tokens_get_length(tokens)+1));
    int num = 0;
    while ((save < tokens_get_length(tokens)) &&
           (strcmp(tokens_get_token(tokens, save), "|") != 0) && 
           (tokens_get_token(tokens, save) != NULL)) {
      
      commands[i][num] = malloc(sizeof(char) * (strlen(tokens_get_token(tokens, save)) + 1));
      commands[i][num] = tokens_get_token(tokens, save) + '\0';
      
      num++;
      save++;
    }
    commands[i][num] = NULL;
    save++;
  }
  return commands;

}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();
    

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}


int main(int argc, char* argv[]) {
  init_shell();

  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  //signal(SIGKILL, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGSTOP, SIG_IGN);
  signal(SIGCONT, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {

      int num_pipes = get_num_pipes(tokens);
      int pipes[num_pipes][2];
      
      for (int i = 0; i < num_pipes; i++) {
        
        if (pipe(pipes[i]) == -1) {
          perror("pipe failed");
          exit(-1);
        }
      }
      pid_t id;
      char*** args = parse_args(num_pipes, tokens);
      for (int i=0; i<num_pipes+1; i++) {
        // int status;
        //pid_t id;
        id = fork();
        if (id == -1) { /* failed fork */
            perror("fork failed");
            exit(-1);
        } else if (id == 0) {

          //pid_t g = setpgrp();
          //tcsetpgrp(id, g);

          signal(SIGINT, SIG_DFL);
          signal(SIGQUIT, SIG_DFL);
          //signal(SIGKILL, SIG_DFL);
          signal(SIGTERM, SIG_DFL);
          signal(SIGSTOP, SIG_DFL);
          signal(SIGCONT, SIG_DFL);
          signal(SIGTTIN, SIG_DFL);
          signal(SIGTTOU, SIG_DFL);


          if (i != 0) {
            dup2(pipes[i-1][0], STDIN_FILENO);
            // close(pipes[i - 1][1]); 
          } 

          if (i != num_pipes) {
            dup2(pipes[i][1], STDOUT_FILENO);
            // close(pipes[i][0]); 
          }

          
          for (int j = 0; j < num_pipes; j++) {
            close(pipes[j][0]);
            close(pipes[j][1]);
          }

          //file redirection
          redirect_files(args[i]);

          //execute program
          execute_cmd(args[i][0], args[i]);

          //if exec failed:
          perror("execv failed");
          exit(-1);
        }
          tcsetpgrp(0, getpid());
        
          // kill(shell_pgid, SIGCONT);
      }

      /* close pipes after child processes finish */
      for (int j = 0; j < num_pipes; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }

       //int status;
      /* Wait for all child processes to finish */
      for (int j = 0; j < num_pipes + 1; j++) {
        wait(NULL);
      }


    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
