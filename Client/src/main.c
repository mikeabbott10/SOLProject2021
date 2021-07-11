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
 * @param dir: the directory we are in
 * @param n_remaining_files: the number of files we still want to write. If -2 then we write every file
 * @param ml: the list of parameters useful for the execution
 * @return 
 *      1 fatal error
 *      0 success
 */
int dirVisit_n_write(DIR *dir, int n_remaining_files, mainList_t ml){
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
                if( n_remaining_files == -2 || n_remaining_files > 0 ){
                    printf( "SubDirectory: %s\n", ptr->d_name );
                    ec( dirVisit_n_write(newdir, n_remaining_files, ml), -1, return -1 );
                }
                ec(closedir(newdir), -1, return -1);
                ec(chdir(".."), -1, return -1);
            }else if(S_ISREG(info.st_mode)){ // not a dir
                if( n_remaining_files != -2 && n_remaining_files-- < 0 ) return 0;
                // write this file to the server
                char* absPath;
                ec( getAbsolutePath(ptr->d_name, &absPath), -1, return -1 );

                opRes=openFile(absPath, O_CREATE|O_LOCK);
                if(stdout_print){ PRINT_INFO("Opening", absPath); }
                if(opRes==0){
                    writeFile(absPath, ml.w_dirname);
                    if(stdout_print){ PRINT_INFO("Writing", absPath); }
                    unlockFile(absPath);
                    if(stdout_print){ PRINT_INFO("Unlocking", absPath); }
                    closeFile(absPath);
                    if(stdout_print){ PRINT_INFO("Closing", absPath); }
                }
                free(absPath);
                msleep(ml.t_time);
            }
        }
        // continua nella dir corrente
        if( n_remaining_files == -2 || n_remaining_files > 0 ) 
            ec( dirVisit_n_write(dir, n_remaining_files, ml), -1, return -1 );
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
    ec( openConnection(ml.f_socketpath, ml.t_time, spec), -1, freeList(ml);return EXIT_FAILURE);
    
    // static order of operations
    // 1: -w option related
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
            dirVisit_n_write(w_dir, (ml.w_n==0)?-2:ml.w_n, ml); // visito la cartella

            ec(chdir(buf), -1, 
                closeConnection(ml.f_socketpath);
                freeList(ml);
                return EXIT_FAILURE;
            ); // torno al path precedente al lavoro svolto
            ec(closedir(w_dir), -1, 
                closeConnection(ml.f_socketpath);
                freeList(ml);
                return EXIT_FAILURE;
            ); // chiudo la cartella che avevo aperto
        }
    }
    // 2: -W option related
    if(ml.W_filenames!=NULL){
        // write these files to the server
        for(i=0; i<ml.W_itemsCount; ++i){
            ec( getAbsolutePath(ml.W_filenames[i], &absPath), -1, freeList(ml);return EXIT_FAILURE );

            opRes=openFile(absPath, O_CREATE|O_LOCK);
            if(stdout_print){ PRINT_INFO("Opening", absPath); }
            if(opRes!=0) continue;
            opRes=writeFile(absPath, ml.w_dirname);
            if(stdout_print){ PRINT_INFO("Writing", absPath); }
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
    //3: -r option related
    if(ml.r_filenames!=NULL){
        // read these files from the server
        char* fileContent = NULL;
        char* fileName = NULL;
        char* lastSlashPtr = NULL;
        size_t contentSize;
        for(i=0; i<ml.r_itemsCount; ++i){
            
            opRes=openFile(ml.r_filenames[i], NO_FLAGS);
            if(stdout_print){ PRINT_INFO("Opening", ml.r_filenames[i]); }
            if(opRes!=0) continue;
            opRes=readFile(ml.r_filenames[i], (void**)&fileContent, &contentSize);
            if(stdout_print){ PRINT_INFO("Reading", ml.r_filenames[i]); }
            //if(opRes!=0) continue; The file was opened, try to close it anyway
            opRes=closeFile(ml.r_filenames[i]);
            if(stdout_print){ PRINT_INFO("Closing", ml.r_filenames[i]); }
            if(opRes!=0) continue;

            if(ml.d_dirname != NULL){
                // get the fileName
                fileName = NULL;
                lastSlashPtr = NULL;
                if((lastSlashPtr = strrchr( ml.r_filenames[i], '/' )) != NULL){
                    // fileName will be the last part of the path
                    fileName = lastSlashPtr+1; // no free needed
                }else{
                    fileName = ml.r_filenames[i]; // no free needed
                }
                // store the new file into the directory
                //TODO: da qua devo uscire col '/' alla fine di absPath
                ec( getAbsolutePath(ml.d_dirname, &absPath), 1, freeList(ml);return EXIT_FAILURE; );
                ec( getFilePath(&absPath, fileName), 1, free(absPath);freeList(ml);return EXIT_FAILURE; );
                ec( writeLocalFile(absPath, fileContent, contentSize), -1, free(absPath);freeList(ml);return EXIT_FAILURE; );
                if(stdout_print){ PRINT_INFO("Storing", absPath); }
                free(absPath);
            }//else printf("File content: %s\n", fileContent);

            free(fileContent);
            msleep(ml.t_time);
        }
    }
    //4: -R option related
    if(ml.R_n != -1){
        readNFiles(ml.R_n, ml.d_dirname);
    }

    ec( closeConnection(ml.f_socketpath), -1, freeList(ml);return EXIT_FAILURE);
    freeList(ml);
    return 0;
}
