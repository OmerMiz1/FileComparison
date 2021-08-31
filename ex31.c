//Omer Mizrachi 313263493 LATE-SUBMISSION
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h> // O_RDONLY

#define IDENTICAL 1
#define DIFFERENT 2
#define SIMILAR 3

enum ExitStatus typedef exit_status;
enum ErrorCode typedef error_code;

/** Main Functions **/
int are_identical(int fd1, int fd2); // returns True if identical
int are_similar(int fd1, int fd2); // returns True if similar

/** Helper Functions**/
int chrcmp(int fd1, int fd2); // compares current 2 chars fd's currently points to
int readable(const char* path); // returns True if readable
long file_size(int fd); // returns file's size
off_t min(off_t size1, off_t size2);
void my_lseek(int fd, off_t count, int from); // avoid checking lseek every time
void print_error(error_code error_code); // prints error message

enum ErrorCode {
    FdOpenError=-20,
    FdCloseError,
    FdNotReadable,
    ReadError,
    NotEnoughArgumentsError,
    LseekError,

    NoError=0,
};
enum ExitStatus{
    SysCallError= -1,
    False = 0,
    True,
    NormalFail,
};

error_code static error = NoError;

int main(int argc, char **argv) {
    int fd1=0, fd2=0, result=0;

    /* Used loop just to be able to break at any point (case of error) */
    while(1) {
        /* Validate arguments count */
        if(argc < 2) {
            print_error(NotEnoughArgumentsError);
        }

        /* Validate files are readable */
        if(!readable(argv[1]) ||
           !readable(argv[2])) {
            error = FdNotReadable;
            break;
        }

        fd1 = open(argv[1], O_RDONLY);
        fd2 = open(argv[2], O_RDONLY);
        if(fd1 < 0 || fd2 < 0) {
            error = FdOpenError;
            break;
        }

        if(are_identical(fd1, fd2)) {
            result = IDENTICAL;
        }
        else if (are_similar(fd1, fd2)) {
            result = SIMILAR;
        } else {
            result = DIFFERENT;
        }
        break;
    } // End of while

    /* Close files */
    if(close(fd1) == -1 || close(fd2) == -1) {
        print_error(FdCloseError);
    }

    /* Error occurred somewhere */
    if(error) {
        print_error(error);
        return SysCallError;
    }

    /* Good exit */
    return result;
}

int are_identical(int fd1, int fd2) {
    off_t size1=0, size2=0;
    int flag=0;

    /* Get sizes (and check for error) */
    size1 = file_size(fd1);
    size2 = file_size(fd2);
    if(error) {
        return SysCallError;
    }

    /* Sizes not equal - files aren't identical */
    if(size1 != size2) {
        return False;
    }

    /* Reset fd to start of file */
    my_lseek(fd1, 0, SEEK_SET);
    my_lseek(fd2, 0, SEEK_SET);
    if(error) {
        return SysCallError;
    }

    /* Compare each char in both files */
    for(int i=0; i < size1; ++i) {
        flag = chrcmp(fd1, fd2);
        switch(flag) {
            case SysCallError: return SysCallError;
            case False: return False;
            case True: continue; // Not necessary just for readability
            default: break;
        }
    }
    return True;
}

int are_similar(int fd1, int fd2) {
    off_t min_cond=0, text_s=0, pattern_s=0, tmp=0, len_to_cmp=0;

    text_s = file_size(fd1);
    pattern_s = file_size(fd2);
    if(error) {
        return SysCallError;
    }

    /* text must be larger than pattern, if not - swap */
    if(pattern_s > text_s) {
        tmp = text_s;
        text_s = pattern_s;
        pattern_s = tmp;
    }

    min_cond = ((pattern_s+1)/2);
    tmp=0;

    /* Iterates big file */
    for(int i=0; i < text_s-1; ++i, tmp=0) {
        /* Small file is on bigger file left boundary */
        if(i==0) {
            for(int j=0; j<pattern_s; ++j, tmp=0) {
                len_to_cmp = pattern_s-j;
                my_lseek(fd1, 0, SEEK_SET);
                my_lseek(fd2, j, SEEK_SET);
                if(error) {
                    return SysCallError;
                }

                /* Compare current offset */
                while(len_to_cmp--) {
                    switch(chrcmp(fd1, fd2)) {
                        case SysCallError: return SysCallError;
                        case True: ++tmp;
                        default: break;
                    }
                }

                /* Current offset meets condition */
                if(tmp >= min_cond) {
                    return True;
                }
            } // end of first case
        } else {
            len_to_cmp = min(text_s-i, pattern_s);
            my_lseek(fd1, i, SEEK_SET);
            my_lseek(fd2, 0, SEEK_SET);
            if(error) {
                return SysCallError;
            }

            /* Compare current offset */
            while(len_to_cmp--) {
                switch(chrcmp(fd1, fd2)) {
                    case SysCallError: exit(1);
                    case True: ++tmp;
                    default: break;
                }
            }
        }

        /* Current offset meets condition */
        if(tmp >= min_cond) {
            return True;
        }
    }
    return False;
}

void my_lseek(int fd, off_t count, int from) {
    if(lseek(fd, count, from) == -1) {
        error = LseekError;
    }
}

off_t file_size(int fd) {
    off_t cur_pos = lseek(fd, 0, SEEK_CUR);
    if(cur_pos == -1) {
        error = LseekError;
        return SysCallError;
    }
    if(lseek(fd, 0, SEEK_SET) == -1) {
        error = LseekError;
        return SysCallError;
    }
    off_t size = lseek(fd, 0, SEEK_END);
    if(size == -1) {
        error = LseekError;
        return SysCallError;
    }

    // return to initial position.
    if(lseek(fd, 0, SEEK_SET) == -1) {
        error = LseekError;
        return SysCallError;
    }

    return size;
}

int chrcmp(int fd1, int fd2) {
    char c1, c2;

    if(read(fd1, &c1, SEEK_CUR) == -1) {
        error = ReadError;
        return SysCallError;
    }
    if(read(fd2, &c2, SEEK_CUR) == -1) {
        error = ReadError;
        return SysCallError;
    }

    return c1 == c2 ? True : False;
}

int readable(const char* path) {
    if(access(path, R_OK)) {
        error = FdNotReadable;
        return SysCallError;
    }
    return True;
}

off_t min(off_t size1, off_t size2) {
    return size1 < size2 ? size1: size2;
}

void print_error(error_code error_code) {
    switch(error_code) {
        case FdOpenError: {
            perror("error: open failed");
            break;
        }
        case FdCloseError:{
            perror("error: close failed");
            break;
        }
        case FdNotReadable:{
            perror("error: file not readable");
            break;
        }
        case ReadError: {
            perror("error: read failed");
            break;
        }
        case NotEnoughArgumentsError:{
            perror("error: not enough arguments error");
            break;
        }
        case LseekError:{
            perror("error: lseek failed");
            break;
        }
        default: break;
    }
}

