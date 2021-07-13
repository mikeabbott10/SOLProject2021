#include<general_utility.h>
#include<cmdLineParser.h>
#include<client_API.h>
#include<conn.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>



//--------- FILES UTIL -------------------------------------------------------------------------------
/**
 * Perform appending operation from file
 * @param filepath: the file we want to get the content and send to the server
 * @param D_dir: the directory for evicted files
 * @return 
 *      -1 fatal error
 *      0 success
 */
int performAppendFromFile(char* filepath, char* D_dir){
    errno = 0;
    char* fileContent;
    size_t contentSize;
    if( getFileContent(filepath, &fileContent, &contentSize) == -1 ) return -1;

    appendToFile(filepath, fileContent, contentSize, D_dir);
    if(stdout_print){ PRINT_INFO("Appending", filepath); };

    free(fileContent);
    return 0;
}

/**
 * @param dir: the directory we are in
 * @param n_remaining_files: the number of files we still want to write. If -2 then we write every file
 * @param ml: the list of parameters useful for the execution
 * @return 
 *      -1 fatal error
 *      0 success
 */
int dirVisit_n_write(DIR *dir, int *n_remaining_files, mainList_t ml){
    int opRes;
    errno = 0;
    struct dirent* ptr = readdir(dir);
    /*Notes on readdir:
        If  the  end  of the directory stream is reached, 
        NULL is returned and errno is not changed.  
        If an error occurs, NULL is returned and errno is set appropriately.
    */
    if(ptr == NULL && errno != 0){ // errore
        perror("Browsing directory");
        return -1;
    }else if(ptr != NULL){ // ptr punta ad un file/dir
        // check file/dir name
        if(strcmp(ptr->d_name,".") != 0 && strcmp(ptr->d_name,"..") != 0){
            struct stat info;
            ec( stat(ptr->d_name, &info), -1, return -1 );
            // if dir go inside
            if (S_ISDIR(info.st_mode)){
                // entra nella directory
                DIR *newdir = openAndCD(ptr->d_name);
                if( *n_remaining_files == -2 || *n_remaining_files > 0 ){
                    //printf( "SubDirectory: %s\n", ptr->d_name );
                    if( dirVisit_n_write(newdir, n_remaining_files, ml) == -1 ) return -1;
                }
                ec(closedir(newdir), -1, return -1);
                ec(chdir(".."), -1, return -1);
            }else if(S_ISREG(info.st_mode)){ // not a dir
                if( *n_remaining_files != -2 && (*n_remaining_files)-- < 0 ) return 0;
                //printf("\t\tn: %d\n", *n_remaining_files);
                // write this file to the server
                char* absPath;
                ec( getAbsolutePath(ptr->d_name, &absPath), -1, return -1 );

                opRes=openFile(absPath, O_CREATE|O_LOCK);
                if(stdout_print){ PRINT_INFO("Opening", absPath); }
                if(opRes==0){
                    opRes=writeFile(absPath, ml.D_dirname);
                    if(stdout_print){ PRINT_INFO("Writing", absPath); }
                    if(opRes!=0){
                        // try appending content to absPath file
                        if( performAppendFromFile(absPath, ml.D_dirname) == -1 ) return -1;
                    }
                    // the file was opened and locked, try to close/unlock it anyway
                    unlockFile(absPath);
                    if(stdout_print){ PRINT_INFO("Unlocking", absPath); }
                    closeFile(absPath);
                    if(stdout_print){ PRINT_INFO("Closing", absPath); };
                }else{
                    // try to perform write only
                    opRes=writeFile(absPath, ml.D_dirname);
                    if(stdout_print){ PRINT_INFO("Writing", absPath); }
                    if(opRes!=0){
                        // try appending content to absPath file
                        if( performAppendFromFile(absPath, ml.D_dirname) == -1 ) return -1;
                    }
                }
                free(absPath);
                msleep(ml.t_time);
            }
        }
        // continua nella dir corrente
        if( *n_remaining_files == -2 || *n_remaining_files > 0 ) 
            if( dirVisit_n_write(dir, n_remaining_files, ml) == -1 ) return -1;
    }
    return 0;
}


//--------- MAIN ------------------------------------------------------------------------------------
int main(int argc, char **argv){
    if(argc<=1){
        print_usage(argv[0]);
        return -1;
    }
    
    /*get list of operations to perform*/
    mainList_t ml;
    initML(&ml);
    if(parseCmdLine(argc, argv, &ml) == EXIT_FAILURE){
        freeList(ml);
        return EXIT_FAILURE;
    }

    /*check if socket path was specified, -D and -w/-W dependency and -d and -r/-R dependency*/
    if(ml.f_socketpath==NULL || (ml.D_dirname!=NULL && ml.w_dirname==NULL && ml.W_filenames == NULL) 
            ||(ml.d_dirname!=NULL && ml.r_filenames==NULL && ml.R_n==-1) || (ml.d_dirname==NULL && ml.R_n!=-1)){
        print_usage(argv[0]);
        freeList(ml);
        return EXIT_FAILURE;
    }

    //need this bc of readN specs :(
    stdout_print = ml.p;

    /*perform operations through API*/
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    spec.tv_sec += 5; /*try for 5 seconds at least*/

    int i;
    char* absPath;
    int opRes;

    /*first operation*/
    ec( openConnection(ml.f_socketpath, ml.t_time, spec), -1, free(sockPath);freeList(ml);return EXIT_FAILURE);
    
    // static order of operations
    // 1: -l option related
    if(ml.l_filenames != NULL){
        for(i=0; i<ml.l_itemsCount; ++i){
            if( getAbsolutePath(ml.l_filenames[i], &absPath) == -1 ){ 
                if(stdout_print){ PRINT_INFO("Unlocking", absPath); }
                continue; 
            }
            opRes=lockFile(absPath);
            if(stdout_print){ PRINT_INFO("Locking", absPath); }
            free(absPath);
            absPath = NULL;
            msleep(ml.t_time);
        }
    }
    // 2: -u option related
    if(ml.u_filenames != NULL){
        for(i=0; i<ml.u_itemsCount; ++i){
            if( getAbsolutePath(ml.u_filenames[i], &absPath) == -1 ){ 
                if(stdout_print){ PRINT_INFO("Unlocking", absPath); }
                continue; 
            }
            opRes=unlockFile(absPath);
            if(stdout_print){ PRINT_INFO("Unlocking", absPath); }
            free(absPath);
            absPath = NULL;
            msleep(ml.t_time);
        }
    }
    // 3: -w option related
    if(ml.w_dirname!=NULL){
        char buf[MAX_PATH_LEN];
        ec(getcwd(buf,MAX_PATH_LEN), NULL, 
            closeConnection(ml.f_socketpath);
            freeList(ml);
            return EXIT_FAILURE;
        ); // save current path

        DIR *w_dir = openAndCD(ml.w_dirname);
        if(w_dir != NULL){
            printf("Directory: %s\n", ml.w_dirname);
            if(ml.w_n == 0) ml.w_n = -2; // visit all subdirectories
            dirVisit_n_write(w_dir, &(ml.w_n), ml); // visito la cartella
            closedir(w_dir); // chiudo la cartella che avevo aperto
        }
        chdir(buf); // torno al path precedente al lavoro svolto
    }
    // 4: -W option related
    if(ml.W_filenames!=NULL){
        // write these files to the server
        for(i=0; i<ml.W_itemsCount; ++i){
            if( getAbsolutePath(ml.W_filenames[i], &absPath) == -1){ continue; }

            opRes=openFile(absPath, O_CREATE|O_LOCK);
            if(stdout_print){ PRINT_INFO("Opening", absPath); }
            if(opRes!=0){ free(absPath); absPath = NULL; continue;}
            opRes=writeFile(absPath, ml.w_dirname);
            if(stdout_print){ PRINT_INFO("Writing", absPath); }
            if(opRes!=0){
                // try appending content to absPath file
                if( performAppendFromFile(absPath, ml.D_dirname) == -1){ 
                    if(stdout_print){ PRINT_INFO("Appending", absPath); }
                    free(absPath); 
                    absPath = NULL; 
                    continue; 
                }
            }
            //if(opRes!=0) continue; The file was opened and locked, try to close and unlock it anyway
            unlockFile(absPath);
            if(stdout_print){ PRINT_INFO("Unlocking", absPath); }
            closeFile(absPath);
            if(stdout_print){ PRINT_INFO("Closing", absPath); }

            free(absPath);
            absPath = NULL;
            msleep(ml.t_time);
        }
    }
    // 5: -r option related
    if(ml.r_filenames != NULL){
        // read these files from the server
        char* fileContent = NULL;
        char* fileName = NULL;
        char* lastSlashPtr = NULL;
        size_t contentSize;
        for(i=0; i<ml.r_itemsCount; ++i){
            if( getAbsolutePath(ml.r_filenames[i], &absPath) == -1){ continue; }

            opRes=openFile(absPath, NO_FLAGS);
            if(stdout_print){ PRINT_INFO("Opening", absPath); }
            if(opRes!=0) continue;
            opRes=readFile(absPath, (void**)&fileContent, &contentSize);
            if(stdout_print){ PRINT_INFO("Reading", absPath); }
            //if(opRes!=0) continue; The file was opened, try to close it anyway
            opRes=closeFile(absPath);
            if(stdout_print){ PRINT_INFO("Closing", absPath); }
            if(opRes!=0) continue;

            if(ml.d_dirname != NULL){
                // get the fileName
                fileName = NULL;
                lastSlashPtr = NULL;
                if((lastSlashPtr = strrchr( absPath, '/' )) != NULL){
                    // fileName will be the last part of the path
                    fileName = lastSlashPtr+1; // no free needed
                }else{
                    fileName = absPath; // no free needed
                }
                // store the new file into the directory
                if( getAbsolutePath(ml.d_dirname, &absPath) == -1){ continue; }
                if( getFilePath(&absPath, fileName) == -1){ free(fileContent);free(absPath); absPath = NULL; continue; }
                if( writeLocalFile(absPath, fileContent, contentSize) == -1){ free(fileContent);free(absPath); absPath = NULL; continue; }
                if(stdout_print){ PRINT_INFO("Storing", absPath); }
            }//else printf("File content: %s\n", fileContent);

            free(absPath);
            absPath = NULL;
            free(fileContent);
            msleep(ml.t_time);
        }
    }
    // 6: -R option related
    if(ml.R_n != -1){
        readNFiles(ml.R_n, ml.d_dirname);
        msleep(ml.t_time);
    }
    
    ec( closeConnection(ml.f_socketpath), -1, freeList(ml);return EXIT_FAILURE);
    freeList(ml);
    return 0;
}
