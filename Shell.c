#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#define MAX_INPUT 1024
#define MAX_ARGS 64

typedef struct {
 int is_background;
 int has_pipe;
 char *in_file;
 char *out_file;
 char *argv_left[MAX_ARGS];
 char *argv_right[MAX_ARGS];
 } ParsedLine;

static void init_parsed_line(ParsedLine *pl)
{
    pl->is_background = 0;
    pl->has_pipe = 0;
    pl->in_file = NULL;
    pl->out_file = NULL;
    pl->argv_left[0] = NULL;
    pl->argv_right[0] = NULL;

}

static void strip_newline(char *s)
{
    s[strcspn(s, "\n")] = '\0';
}

static int is_blank_line(const char *s)
{
    if(strlen(s) == 0){
        return 1;
    }
    for(int i = 0; i < strlen(s); i++){
        if(s[i] != ' '){
            return 0;
        }
    }
    return 1;
}

static void normalize_input(const char *input, char *output, size_t out_size)
{
    int length = strlen(input);
    int i = 0,  j = 0;

    while(i < length && i < out_size - 1){ 
    
        if(input[i] == '|' || input[i] == '<' || input[i] == '>' || input[i] == '&'){ // check for valid operator
            if(i > 0 && input[i-1] != ' '){ // if i > 0 and the character before operator isn't a space add a space in output                                                          isn't
                output[j++] = ' ';
            } 

            output[j++] = input[i]; // in output make it equal to the operator

            if(input[i+1] != ' '){ // if the input infornt of operator is not a space make it a space in output
                output[j++] = ' '; 
            }
        } else{
            output[j++] = input[i]; // if not an operator just make output equal to input
        }
      
        i++; // increment i
    }
    output[j] = '\0';
}

static int parse_line(char *line, ParsedLine *pl)
{
    char *op[30]; // variable to store operator
    int count = 0; // count for number of operators
    int i = 0; // count for left side of argv
    int j = 0; // count for right side of argv

    char *token = strtok(line, " \t");
    while (token != NULL){ // while ptr doesnt return NULL

        // checking to make sure & is at the end
        if(*token == '&' || *op[0] == '&'){
                if(i == 0 || count > 0){
                    fprintf(stderr, "Error: & not at end of command.\n");
                    return 1; // return 1 for error
                }
        }  

        /* if token equals any of the valid operators it will check to see if 
        another operator was found and will return if it did if not then will store the operator*/
        if(*token == '|' || *token == '<' || *token == '>' || *token == '&'){

            if(count > 0){ // if there is more than 1 operator in a command will return with an error
                fprintf(stderr, "Error:  number of operator exceeds limit.\n");
                return 1; // return 1 for error
            }

            op[count++] = token; // store operator token into op variable for later and increment count

        } 
        else if(count > 0){
            pl->argv_right[j++] = token; // insert token into right argv
        }
        else{
            pl->argv_left[i++] = token; // insert token into left argv
        }

        token = strtok(NULL,  " \t"); // give token NULL termminator

    }

    pl->argv_left[i] = NULL;
    pl->argv_right[j] = NULL;

    if(count > 0){ // make sure there is an operator found
        if(*op[0] == '|'){
            if(pl->argv_left[0] == NULL || pl->argv_right[0] == NULL){
                fprintf(stderr, "Error: pipe requires a command on both sides.\n");
                return 1; // return 1 for error
            }
            pl->has_pipe = 1;
        }
        else if(*op[0] == '<'){
            if(pl->argv_right[0] == NULL){
                fprintf(stderr, "Error: input redirection requires a file name.\n");
                return 1; // return 1 for error
            }
            pl->in_file = pl->argv_right[0];
        }
        else if(*op[0] == '>'){
            if(pl->argv_right[0] == NULL){
                fprintf(stderr, "Error: input redirection requires a file name.\n");
                return 1; // return 1 for error
            }
            pl->out_file = pl->argv_right[0];
        }
        else if(*op[0] == '&'){
            pl->is_background = 1;
        }
    }
    return 0; // return 0 for success
}

static void apply_redirection(const ParsedLine *pl) {
    // input redirection: wc < file.txt
    if (pl->in_file != NULL) {
        int fd = open(pl->in_file, O_RDONLY); // open file for reading
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        dup2(fd, STDIN_FILENO); // point stdin at the file
        close(fd); // done with original fd
    }

    // output redirection: ls > out.txt
    if (pl->out_file != NULL) {
        int fd = open(pl->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO); // point stdout at the file
        close(fd);
    }
}

static void execute_simple_command(const ParsedLine *pl)
{
    // Empty
    if (pl->argv_left[0] == NULL) {
        fprintf(stderr, "Error: empty command\n");
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return;
    }

    // child process
    if (pid == 0) {
        apply_redirection(pl); // set up redirections
        execvp(pl->argv_left[0], pl->argv_left);
        perror("execvp"); // only runs if execvp failed
        exit(1);

    // parent process
    } else {
        if (pl->is_background) {
            printf("[running in background]\n"); // move on
        } else {
            int status;
            if (waitpid(pid, &status, 0) < 0) { // wait for child to be done
                perror("waitpid");
            }
        }
    }
}

static void execute_pipe_command(const ParsedLine *pl){
    // Validation
    if (pl->argv_left[0] == NULL || pl->argv_right[0] == NULL) {
        fprintf(stderr, "Error: pipe requires a command on both sides.\n");
        return;
    }

    int pfd[2]; // pfd[0] = read end, pfd[1] = write end
    if (pipe(pfd) < 0) {
        perror("pipe");
        return;
    }

    // fork left child (writes into pipe)
    pid_t pid_left = fork();
    if (pid_left < 0) {
        perror("fork");
        close(pfd[0]);
        close(pfd[1]);
        return;
    }

    if (pid_left == 0) {
        dup2(pfd[1], STDOUT_FILENO); // wire stdout to pipe write end
        close(pfd[0]); // dont need read end
        close(pfd[1]); // dup2 copied it, close original
        execvp(pl->argv_left[0], pl->argv_left);
        perror("execvp");
        exit(1);
    }

    // fork right child (reads from pipe)
    pid_t pid_right = fork();
    if (pid_right < 0) {
        perror("fork");
        close(pfd[0]);
        close(pfd[1]);
        waitpid(pid_left, NULL, 0); // clean up left child
        return;
    }

    if (pid_right == 0) {
        dup2(pfd[0], STDIN_FILENO); // wire stdin to pipe read end
        close(pfd[1]); // dont need write end
        close(pfd[0]); // dup2 copied it, close original
        execvp(pl->argv_right[0], pl->argv_right);
        perror("execvp");
        exit(1);
    }

    // parent closes both ends
    close(pfd[0]);
    close(pfd[1]);

    if (pl->is_background) {
        printf("[running in background]\n");
    } else {
        int status;
        if (waitpid(pid_left, &status, 0) < 0) { // wait for both children
            perror("waitpid");
        }
        if (waitpid(pid_right, &status, 0) < 0) {
            perror("waitpid");
        }
    }
}

static void reap_background_children(void)
{
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
    }
}

int main(void)
{
    char input[MAX_INPUT];
    char normalized[MAX_INPUT * 2]; 
    ParsedLine pl; // Holds parsed info

    while (1) {
        reap_background_children(); // clean up any finished background jobs

        /* print prompt and flush */
        printf("myshell> ");
        fflush(stdout);

        /* read a line with fgets; handle EOF */
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }

        /* strip newline */
        strip_newline(input);

        /* if empty line, continue */
        if (is_blank_line(input)) {
            continue;
        }

        /* if input == "exit", print message and break */
        if (strcmp(input, "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        }


        // Set all fields to safe on default
        init_parsed_line(&pl);

        // Space out the operators
        normalize_input(input, normalized, sizeof(normalized));

        // Parse into ParsedLine struct
        if (parse_line(normalized, &pl) != 0) {
            continue;
        }

        // Dispatch to the right executor
        if (pl.has_pipe) {
            execute_pipe_command(&pl);
        } else {
            execute_simple_command(&pl);
        }
    }

    return 0;
}