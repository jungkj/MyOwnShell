#define _POSIX_C_SOURCE 200809L // required for strdup() on cslab
#define _DEFAULT_SOURCE // required for strsep() on cslab
#define _BSD_SOURCE // required for strsep() on cslab

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "pid_list.h"

#define MAX_ARGS 32

void redirection(char *output_file, char *input_file, char *append_file);
int get_re(char **command_line_words, size_t num_args, char **output_file, char **input_file, char **append_file);
void clear(char **output_file, char **input_file, char **append_file);
void command(char **command_line_words, size_t num_args, node *list_node);

char **get_next_command(size_t *num_args)
{
    // print the prompt
    printf("cssh$ ");

    // get the next line of input
    char *line = NULL;
    size_t len = 0;
    getline(&line, &len, stdin);
    if (ferror(stdin))
    {
        perror("getline");
        exit(1);
    }
    if (feof(stdin))
    {
        return NULL;
    }

    // turn the line into an array of words
    char **words = (char **)malloc(MAX_ARGS*sizeof(char *));
    int i=0;

    char *parse = line;
    while (parse != NULL)
    {
        char *word = strsep(&parse, " \t\r\f\n");
        if (strlen(word) != 0)
        {
            words[i++] = strdup(word);
        }
    }
    *num_args = i;
    for (; i<MAX_ARGS; ++i)
    {
        words[i] = NULL;
    }

    // all the words are in the array now, so free the original line
    free(line);

    return words;
}

void free_command(char **words)
{
    for (int i=0; i<MAX_ARGS; ++i)
    {
        if (words[i] == NULL)
        {
            break;
        }
        free(words[i]);
    }
    free(words);
}

void redirection(char *output_file, char *input_file, char *append_file){
    if (input_file != NULL){
        int read_fd = open(input_file, O_RDONLY);
        if (read_fd == -1){
            perror("open");
            exit(1);
        }
        if (dup2(read_fd, STDIN_FILENO)==-1){
            perror("dup2");
            exit(1);
        }
        close(read_fd);
    }
    if (output_file !=NULL){
        int write_fd = open(output_file, O_WRONLY| O_TRUNC | O_CREAT, 0644);
        if (write_fd == -1){
            perror("write");
            exit(1);
        }
        if(dup2(write_fd, STDOUT_FILENO)==-1){
            perror("dup2");
            exit(1);
        }
        close(write_fd);
    if (append_file != NULL){
        int write_fd = open(append_file, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (write_fd == -1){
            perror("write");
            exit(1);
        }
        if (dup2(write_fd, STDOUT_FILENO)==-1){
            perror("dup2");
            exit(1);
        }
        close(write_fd);
    }
    }

}

int get_re(char **command_line_words, size_t num_args, char **output_file, char **input_file, char **append_file){
    int stdin_count = 0;
    int stdout_count = 0;
    int stdapp_count = 0;

    for (int i =0; i < num_args; ++i){
        if (strcmp(command_line_words[i], "<")==0){
            stdin_count++;
            *input_file = command_line_words[i+1];
            free(command_line_words[i]);
            command_line_words[i] = NULL;
        }

        else if (strcmp(command_line_words[i], ">")==0){
            stdout_count++;
            *output_file = command_line_words[i+1];
            free(command_line_words[i]);
            command_line_words[i] = NULL;
        }

        else if (strcmp(command_line_words[i], ">>")==0){
            stdapp_count++;
            *append_file = command_line_words[i+1];
            free(command_line_words[i]);
            command_line_words[i]=NULL;
        }
    }
    if (stdin_count > 1){
        printf("Error, Can't have two <'s\n");
        return 0;
    }

    else if (stdout_count > 1){
        printf("Error, can't have two >'s\n");
        return 0;
    }

    else if (stdapp_count > 1){
        printf("Error, can't have two >>'s\n");
        return 0;
    }

    else if (stdapp_count + stdout_count > 1){
        printf("Error, can't have two >'s or two >>'s\n");
        return 0;
    }
    return 1;

    }

void clear(char **output_file, char **input_file, char **append_file){
    if (*output_file != NULL){
        output_file = NULL;
    }

    if (*input_file !=NULL){
        input_file = NULL;
    }

    if (*append_file != NULL){
        append_file = NULL;
    }
}

void command(char **command_line_words, size_t num_args, node *list_node){
    char *output_file = NULL;
    char *input_file = NULL;
    char *append_file = NULL;
    int var = 0;

    int redirect_result = get_re(command_line_words, num_args, &output_file, &input_file, &append_file);
    if (redirect_result == 0){
        return;
    }

    if (strcmp(command_line_words[num_args-1], "&")==0){
        command_line_words[num_args-1]=NULL;
        var = 1;
    }

    pid_t fork_rv = fork();
    if (fork_rv == -1){
        perror("fork");
        exit(1);
    }

    if(fork_rv == 0){
        redirection(output_file, input_file, append_file);
        execvp(command_line_words[0], command_line_words);
        perror("execvp");
        exit(1);
    }
    clear(&output_file, &input_file, &append_file);

    if(fork_rv != 0 && var){
        add_node(list_node, fork_rv);
    }

    else if (fork_rv !=0 && var ==0){
        if (waitpid(fork_rv, NULL, 0) == -1){
            perror("wait");
            exit(1);
        }
    }

    node *curr = list_node ->next; 
    while (curr != list_node){
        pid_t wait = waitpid(curr->pid, NULL, WNOHANG);
        if(wait == -1){
            perror("waitpid");
            exit(1);
        }
        else if(wait == 0){
            curr = curr->next;
        }
        else{
            curr = curr->next;
            remove_node(list_node, curr->prev->pid);
        }
    }
    if ((fork_rv !=0) && (var ==0)){
        if(waitpid(fork_rv, NULL, 0)==1){
            perror("wait");
            exit(1);
        }
    }
    command_line_words[num_args-1] = NULL;
    free_command(command_line_words);
}

int main()
{
    size_t num_args;

    // get the next command
    char **command_line_words = get_next_command(&num_args);
    node *list_node = new_list();
    while (command_line_words != NULL)
    {
        // run the command here
        // don't forget to skip blank commands
        // and add something to stop the loop if the user 
        // runs "exit"
	
	//check if blank command
        if (command_line_words[0] == NULL || strcmp(command_line_words[0], "")==0){
            command_line_words = get_next_command(&num_args);
            continue;
        }	
        //check if user ran "exit"
        else if (strcmp(command_line_words[0], "exit") == 0){
            break;
        }
        command(command_line_words, num_args, list_node);
        command_line_words = get_next_command(&num_args);
    }

    // free the memory for the last command
    free_list(list_node);
    free_command(command_line_words);

    return 0;
}
