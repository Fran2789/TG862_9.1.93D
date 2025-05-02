#if !defined ERRORS_H
#define ERRORS_H

#define err_die(error, quiet) if(!quiet) {perror(error); exit(1);};

#define err_print(error, quiet) if(!quiet) perror(error);

typedef enum
{
    N_OK = 0,
    OK,
    MALLOC_FAILED 
}VALUE_STATUS;

#endif /* ERRORS_H */
