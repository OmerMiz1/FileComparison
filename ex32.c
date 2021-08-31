//Omer Mizrachi 313263493 LATE-SUBMISSION
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/fcntl.h> // O_RDONLY
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>
#include <wait.h>

#define OVERRIDE_WRITE (O_WRONLY | O_CREAT | O_TRUNC)
#define STD_ERR 2
#define RESULT_FILE "results.csv"
#define REGEX_C "^.+\\.c$"
#define REGEX_C_TXT "^.+(\\.c|\\.txt)$"
#define TMP_OUT "cur_output.txt"
#define TIMEOUT 3
#define MAX_ERR 1024
#define MAX_GRADE_LEN 4

#define IDENTICAL 1
#define DIFFERENT 2
#define SIMILAR 3

enum ExitStatus typedef exit_status;
enum Grade typedef grade;
enum ErrorCode typedef error_code;

/** Main Functions **/
void test_main_dir(char config[3][PATH_MAX]);
int test_sub_dir(char config[3][PATH_MAX], char *dir_name);
int compile(char *dir_path, char *file_name);
int execute(char *args[], char* input_path, unsigned timeout, int redirect);
int run(char* dir_path, char input_path[PATH_MAX], char* file_name);
int compare(char* file1, char* file2);
int save_to_csv(char *user_name, grade user_grade, int csv_fd);

/** Helper Functions**/
int get_conf_data(char data_buf[3][PATH_MAX], int fd);
int is_valid_dir(char *arg);
int is_valid_file(char *arg);
int is_valid_config(char conf[3][PATH_MAX]);
off_t file_size(int fd);
void redirect_std_io(char new_stdin[PATH_MAX], char new_stdout[PATH_MAX]);
struct DIR* my_opendir(char* path);
int my_closedir(DIR* dir);
struct dirent* my_readdir(struct DIR* dir);
void print_error(error_code error_code);
error_code cur_error(error_code new_code, int first_access);
int my_write(int fd, char msg[MAX_ERR]);

    enum ExitStatus {
    SysCallError = -1,
    False = 0,
    True,
};
enum Grade {
    ErrGrade=-1,
    NoCFile=0,
    CompilationError=10,
    TimedOut=20,
    Wrong=50,
    Similar=75,
    Excellent=100
};
enum ErrorCode {
    NotEnoughArgs=-30,
    OpenDirError,
    CloseDirError,
    OpenFdError,
    CloseFdError,
    ReadFdError,
    WriteFdError,
    StatError,
    Dup2Error,
    SignalError,
    LseekError,
    RegCompError,
    FileNotReadableError,
    DirNotReadableError,
    NoSuchGrade,
    GetCwdError,
    ForkError,
    ExecError,
    WaitPidError,
    ChildExitError,
    NotValidDir,
    InputFileNotExist,
    OutputFileNotExist,
    DeleteFileError,


    NoError=0
};

int main(int argc, char **argv) {
    char config[3][PATH_MAX] = {"\0"};
    int fd=0;
    cur_error(NoError, 1);

    /* Check path exists & readable*/
    if(argc != 2) {
        print_error(NotEnoughArgs);
        exit(SysCallError);
    }

    if(!is_valid_file(argv[1])) {
        print_error(cur_error(NoError,0));
        exit(SysCallError);
    }

    /* Open file & parse data into an array*/
    fd = open(argv[1], O_RDONLY, 0777);
    if(fd == SysCallError) {
        print_error(OpenFdError);
        exit(SysCallError);
    }
    if(get_conf_data(config, fd) == SysCallError) {
        print_error(cur_error(NoError,0));
        if(close(fd) == SysCallError) {
            print_error(CloseFdError);
        }
        exit(SysCallError);
    }
    if(close(fd) == SysCallError) {
        print_error(CloseFdError);
        exit(SysCallError);
    }

    /* Validate configuration file paths are valid */
    if(!is_valid_config(config)) {
        print_error(cur_error(NoError,0));
        exit(0);
    }

    test_main_dir(config);

    return 0;
}

void test_main_dir(char config[3][PATH_MAX]) {
    char sub_dir_path[PATH_MAX]={"\0"}, dir_path[PATH_MAX]={"\0"};
    struct DIR *dir=0;
    struct dirent* dir_ent=0;
    grade cur_grade=0;
    int csv_fd=0;
    error_code err;

    /* Main dir path*/
    strcpy(dir_path, config[0]);
    dir = my_opendir(dir_path);
    err = cur_error(NoError,0);
    if(err) {
        print_error(err);
        exit(SysCallError);
    }

    /* Create csv file */
    csv_fd = open(RESULT_FILE, OVERRIDE_WRITE | O_SYNC, 0777);
    if(csv_fd == SysCallError) {
        print_error(OpenFdError);
        exit(SysCallError);
    }

    while((dir_ent = my_readdir(dir)) != NULL) {
        /* Generate path for sub-directory */
        strcpy(sub_dir_path, dir_path);
        strcat(sub_dir_path, dir_ent->d_name);

        cur_grade = test_sub_dir(config, dir_ent->d_name);
        if(cur_grade == SysCallError) {
            print_error(cur_error(NoError,0));
        }
        if(save_to_csv(dir_ent->d_name, cur_grade, csv_fd) == SysCallError) {
            print_error(cur_error(NoError,0));
        }
    }
    if(closedir((DIR*) dir) == SysCallError) {
        print_error(CloseDirError);
    }
}

int test_sub_dir(char config[3][PATH_MAX], char *dir_name) {
    char dir_path[PATH_MAX]={"\0"}, in_path[PATH_MAX]={"\0"}, correct_out_path[PATH_MAX]={"\0"};
    struct DIR* dir=0;
    struct dirent* dir_ent=0;
    regex_t c_file_rx;
    int reti=0;
    grade result=0;
    error_code err=NoError;

    /* Currently tested dir path */
    strcpy(dir_path, config[0]);
    strcat(dir_path, "/");
    strcat(dir_path, dir_name);

    /* Input & correct output paths */
    strcpy(in_path, config[1]);
    strcpy(correct_out_path, config[2]);

    /* Open current tested dir */
    dir = my_opendir(dir_path);
    if((err=cur_error(NoError,0))) {
        cur_error(err,0);
        return SysCallError;
    }

    reti = regcomp(&c_file_rx, REGEX_C,REG_EXTENDED);
    if(reti) {
        cur_error(RegCompError,0);
        return SysCallError;
    }

    /* Search for '.c' file in current directory */
    while((dir_ent = my_readdir(dir)) != NULL) {
        reti = regexec(&c_file_rx, dir_ent->d_name, 0 ,NULL, 0);
        if(!reti) {
            /* File found */
            regfree(&c_file_rx);
            break;
        }
        else {
            continue;
        }
    }

    /* No c file (grade 0) */
    if(dir_ent == NULL) {
        my_closedir((DIR *) dir);
        if((err=cur_error(NoError,0))) {
            print_error(err);
        }
        return NoCFile;
    }

    /* Compile file (grade 10 if fails) */
    switch(compile(dir_path ,dir_ent->d_name)) {
        case SysCallError: result = SysCallError; break;
        case 1: result = CompilationError; break;
        default: break;
    }

    /* Run file (grade 20 if times out)*/
    if(!result) {
        switch(run(dir_path, in_path, "a.out")) {
            case SysCallError: result = SysCallError; break;
            case TimedOut: result = TimedOut; break;
            default: break;
        }
    }

    /* Compare output with correct output (grade 50-100) */
    if(!result) {
        result = compare(TMP_OUT, correct_out_path);
        if(remove(TMP_OUT) == SysCallError) {
            print_error(DeleteFileError);
        }
    }

    if(my_closedir((DIR *) dir) == SysCallError) {
        print_error(CloseDirError);
    }

    return result;
}

/** Function returns child's exit status. On error returns SysCallError and set global error
 * */
int execute(char *args[], char input_path[PATH_MAX], unsigned timeout, int redirect) {
    pid_t pid;
    exit_status status=0;
    error_code err=0;

    pid = fork();
    /* Error */
    if(pid < 0) {
        cur_error(ForkError, 0);
        return SysCallError;
    }
    /* Child - execute file */
    else if(!pid) {
        if(redirect) {
            redirect_std_io(input_path, TMP_OUT);
            if((err=cur_error(NoError, 0))) {
                print_error(err);
                exit(SysCallError);
            }
        }
        if(timeout)
            alarm(timeout);
        execv(args[0], args);

        /* Error */
        print_error(ExecError);
        exit(SysCallError);
    }

    /* Parent - Wait for child to finish execution */
    else {
        if(waitpid(pid, (int *) &status, WCONTINUED) != pid) {
            cur_error(WaitPidError,0);
            return SysCallError;
        }
    }

    if(status == SIGALRM) {
        return TimedOut;
    }

    /* Child exited successfully, return exit status */
    if(WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    /* Child failed (in exit or in general) or something else went wrong.*/
    cur_error(ChildExitError,0);
    return SysCallError;
}

/** Function return value depends on execute's return value
 * */
int compile(char *dir_path, char *file_name) {
    char target[PATH_MAX], source[PATH_MAX];

    /* Target file */
    strcpy(target, dir_path);
    strcat(target, "/a.out");

    /* Source file */
    strcpy(source, dir_path);
    strcat(source, "/");
    strcat(source, file_name);

    /* Set args & execute (NO timeout) */
    char *args[] = {"/usr/bin/gcc", "-o", target, source, NULL};
    return execute(args,NULL, 0, 0);
}

/** Function return value depends on execute's return value
 * */
int run(char* dir_path,char input_path[PATH_MAX], char* file_name) {
    char* target[PATH_MAX];
    int retval=0;

    /* Generate file's path */
    strcpy((char *) target, dir_path);
    strcat((char *) target, "/");
    strcat((char *) target, file_name);

    /* Set args & execute with timeout */
    char *args[] = {(char *) target, NULL};
    retval = execute(args, input_path,  TIMEOUT, 1);
    if(retval == 14) {
        return TimedOut;
    }
    return retval;
}

/** Function returns a grade or SysCallError\NoSuchGrade if error happens
 * */
int compare(char* file1, char* file2) {
    char comp_path[PATH_MAX]={"\0"};

    /* Assuming '/comp.out' in same directory */
    strcat(comp_path, "./comp.out");

    /* Set args & execute comparison with timeout */
    char *args[] = {comp_path, file1, file2};
    switch(execute(args,NULL, TIMEOUT, 0)) {
        case SysCallError: return SysCallError;
        case IDENTICAL: return Excellent;
        case DIFFERENT: return Wrong;
        case SIMILAR: return Similar;
        default: return NoSuchGrade;
    }
}

/** Function returns Flase when ok, else returns SysCallError and set global error
 * */
int save_to_csv(char *user_name, grade user_grade, int csv_fd) {
    char to_save[PATH_MAX*2]={"\0"}, reason[PATH_MAX]={"\0"}, grade[MAX_GRADE_LEN]={"\0"};

    /* Parse grade to string */
    sprintf(grade, "%d", user_grade);

    strcpy(to_save, user_name);
    strcat(to_save, ",");

    /* Reason & Grade */
    switch(user_grade) {
        case NoCFile: {
            strcpy(reason, "NO_C_FILE");
            break;
        }
        case CompilationError: {
            strcpy(reason, "COMPILATION_ERROR");
            break;
        }
        case TimedOut: {
            strcpy(reason, "TIMEOUT");
            break;
        }
        case Wrong: {
            strcpy(reason, "WRONG");
            break;
        }
        case Similar: {
            strcpy(reason, "SIMILAR");
            break;
        }
        case Excellent: {
            strcpy(reason, "EXCELLENT");
            break;
        }
        default: {
            cur_error(NoSuchGrade,0);
            return SysCallError;
        }
    }

    strcat(to_save, reason);
    strcat(to_save, ",");
    strcat(to_save, grade);
    strcat(to_save, "\n");

    if(write(csv_fd, to_save, strlen(to_save)) == SysCallError) {
        cur_error(WriteFdError,0);
        return SysCallError;
    }

    return False;
}

/** Function returns True if ok, if False recieved need to check global error
 * */
int is_valid_dir(char *arg) {
    struct stat info;

    /* check if file is not regular or not readable */
    if(stat(arg, &info) == SysCallError) {
        cur_error(StatError,0);
        return False;
    }
    if(!S_ISDIR(info.st_mode)) {
        return False;
    }
    if(access(arg, R_OK) == SysCallError) {
        print_error(DirNotReadableError);
        return False;
    }

    return True;
}

/** Function returns True if ok, otherwise returns False. If False recieved user must check global error
 * */
int is_valid_file(char *arg) {
    struct stat info;
    regex_t file_rx;
    int reti;

    /* Compile regex */
    reti = regcomp(&file_rx, REGEX_C_TXT, REG_EXTENDED);
    if(reti) {
        cur_error(RegCompError,0);
        return False;
    }

    /* Check if file is regular and readable */
    if(stat(arg, &info) == SysCallError) {
        cur_error(StatError,0);
        return False;
    }
    if(!S_ISREG(info.st_mode)) {
        return False;
    }
    if(access(arg, R_OK) == SysCallError) {
        print_error(FileNotReadableError);
        return False;
    }

    /* Return true if file ends with '.c' or '.txt' */
    reti = regexec(&file_rx, arg, 0, NULL, 0);
    if(!reti) {
        return True;
    } else {
        return False;
    }
}

/** Function returns True (1) when ok, otherwise returns False and user must check global error
 * */
int is_valid_config(char conf[3][PATH_MAX]) {
    /* First Path */
    if(!is_valid_dir(conf[0])) {
        cur_error(NotValidDir,0);
        return False;
    }

    /* Second Path */
    if(!is_valid_file(conf[1])) {
        cur_error(InputFileNotExist,0);
        return False;
    }

    /* Third Path */
    if(!is_valid_file(conf[2])) {
        cur_error(OutputFileNotExist,0);
        return False;
    }

    /* Return true for valid */
    return True;
}

/** Function returns False (0) for good ret val, otherwise SysCallError (-1)
 * */
int get_conf_data(char data_buf[3][PATH_MAX], int fd) {
    char read_buf[4*PATH_MAX]={"\0"}, *token=NULL;
    int bytes_read=0, total_bytes_read=0;
    error_code err=0;

    /* Get file size (and check for error) */
    off_t size = file_size(fd);
    if((err=cur_error(NoError,0))) {
        cur_error(err,0);
        return SysCallError;
    }

    do {
        bytes_read = read(fd, read_buf,size);

        if(bytes_read == SysCallError) {
            cur_error(ReadFdError,0);
            return SysCallError;
        }

        total_bytes_read += bytes_read;
    } while(total_bytes_read < size);

    token = strtok(read_buf, "\n");

    /* Read buffered file into split lines */
    for(int i=0; token!=NULL; ++i, token = strtok(NULL, "\n")) {
        strcpy(data_buf[i], token);
    }

    /* Good ret val */
    return False;
}

/** Function returns SysCallError if error happens
 * */
off_t file_size(int fd) {
    off_t cur_pos, size;

    /* Save curr offset */
    cur_pos = lseek(fd, 0, SEEK_CUR);
    if(cur_pos == SysCallError) {
        cur_error(LseekError,0);
        return SysCallError;
    }

    /* Reset to start and get number of chars read with SEEK_END */
    if(lseek(fd, 0, SEEK_SET) == SysCallError) {
        cur_error(LseekError,0);
        return SysCallError;
    }
    size = lseek(fd, 0, SEEK_END);
    if(size == SysCallError) {
        cur_error(LseekError,0);
        return SysCallError;
    }

    /* Return to initial position */
    if(lseek(fd, cur_pos, SEEK_SET) == SysCallError) {
        cur_error(LseekError,0);
        return SysCallError;
    }

    return size;
}

/** Function may turn global error - must check after use
 * */
void redirect_std_io(char new_stdin[PATH_MAX], char new_stdout[PATH_MAX]) {
    int fd1, fd2;

    /* stdin override */
    fd1 = open(new_stdin, O_RDONLY, 0777);
    if(fd1 == SysCallError) {
        cur_error(OpenFdError,0);
        return;
    }

    /* stdout override */
    fd2 = open(new_stdout, O_RDWR | O_CREAT | O_TRUNC, 0777);
    if(fd2 == SysCallError) {
        close(fd1);
        cur_error(OpenFdError,0);
        return;
    }

    /* Redirect */
    if (dup2(fd1, 0) == SysCallError) {
        close(fd1);
        close(fd2);
        cur_error(Dup2Error,0);
        return;
    }
    if (dup2(fd2, 1) == SysCallError) {
        close(fd1);
        close(fd2);
        cur_error(Dup2Error,0);
        return;
    }

}

/** Must check if re val is NULL, if it is - must check global error
 * */
struct DIR* my_opendir(char* path) {
    DIR* result;

    /* Reset errno before open */
    errno = 0;
    result = opendir(path);

    if(result == NULL && errno) {
        cur_error(OpenDirError,0);
    }

    return (struct DIR *) result;
}

int my_closedir(DIR* dir) {
    if(closedir(dir) == SysCallError) {
        cur_error(CloseDirError,0);
        return SysCallError;
    }

    /* Good ret val */
    return False;
}

struct dirent* my_readdir(struct DIR* dir) {
    struct dirent* result;

    /* Skip prev and current directories */
    do {
        result = readdir((DIR *) dir);
        if(result == NULL) {
            return result;
        }
        if(!strcmp(result->d_name, "..")) {
            continue;
        }
        if(!strcmp(result->d_name, ".")) {
            continue;
        }
        break;
    } while(1);
    return result;
}

void print_error(error_code error_code) {
    switch(error_code) {
        case NotEnoughArgs:my_write(STD_ERR, "Not enough arguments\n");break;
        case OpenDirError:my_write(STD_ERR, "opendir failed\n");break;
        case CloseDirError:my_write(STD_ERR, "closedir failed\n");break;
        case OpenFdError:my_write(STD_ERR, "open failed\n");break;
        case CloseFdError:my_write(STD_ERR, "close failed\n");break;
        case ReadFdError:my_write(STD_ERR, "read failed\n");break;
        case WriteFdError:break; // if write fails dont print anything
        case StatError:my_write(STD_ERR, "stat failed\n");break;
        case Dup2Error:my_write(STD_ERR, "dup2 failed\n");break;
        case SignalError:my_write(STD_ERR, "signal failed\n");break;
        case LseekError:my_write(STD_ERR, "lseek failed\n");break;
        case RegCompError:my_write(STD_ERR, "regcomp failed\n");break;
        case FileNotReadableError:my_write(STD_ERR, "file is not readable (access)\n");break;
        case DirNotReadableError:my_write(STD_ERR, "directory is not readable (access)\n");break;
        case NoSuchGrade:break; // DO NOTHING JUST FOR ME TO TEST THINGS
        case GetCwdError:my_write(STD_ERR, "getcwd failed\n");break;
        case ForkError:my_write(STD_ERR, "fork failed\n");break;
        case ExecError:my_write(STD_ERR, "execv failed\n");break;
        case WaitPidError:my_write(STD_ERR, "waitpid failed\n");break;
        case ChildExitError:break; // DO NOTHING, JUST FOR ME TO TEST
        case NotValidDir:my_write(STD_ERR, "Not a valid directory\n");break;
        case InputFileNotExist:my_write(STD_ERR, "Input File not exist\n");break;
        case OutputFileNotExist:my_write(STD_ERR, "Output File not exist\n");break;
        case DeleteFileError:my_write(STD_ERR, "delete file failed\n");break;
        case NoError: //DO NOTHING
        default: break;
    }
}

int my_write(int fd, char msg[MAX_ERR]) {
    size_t count = strlen(msg);
    error_code retval = NoError;

    if(write(fd,msg,count) == SysCallError) {
        retval = cur_error(WriteFdError, 0);
    }

    return retval;
}

error_code cur_error(error_code new_code, int first_access) {
    static error_code current_error;
    error_code retval = NoError;

    if(!first_access) {
        retval = current_error;
        current_error = new_code;
    } else {
        current_error = NoError;
    }

    return retval;
}
