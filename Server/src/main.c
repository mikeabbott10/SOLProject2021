#include<server_config.h>
#include<stdio.h>
#include<stdlib.h>
#include<general_utility.h>
#include<connection_util.h>

pthread_t spawn_signal_thread();
void* signal_handler_fun(void*);
void quit();

int main(int args, char** argv){
    atexit(quit);

    /*get information from config file*/
    ec(parseConfigFile("./src/config.txt"), EXIT_FAILURE, return EXIT_FAILURE);
    /*create the client buffer, it will be shared between master and workers*/
    clientBuffer = client_buffer_create(config_struct.max_connections);
    /*initialize the signal mask*/
    procCurrentMask = initSigMask(); /*if it fails exit(EXIT_FAILURE) is called*/
    /*start signal handler thread*/
    pthread_t sig_handler_tid;
    ec_n( pthread_create(&sig_handler_tid, NULL, signal_handler_fun, NULL), 0, return EXIT_FAILURE );

    pthread_join(sig_handler_tid, NULL);
    return EXIT_SUCCESS;
}

/*
    Overview: it keeps listening to incoming signals
*/
void * signal_handler_fun(void* p){
    int sig;
    while(1){
        sigwait(&procCurrentMask, &sig); // non funziona se il segnale è destinato direttamente a un thread
        if(sig == SIGINT || sig == SIGQUIT){
            // TODO: termina il prima possibile, ossia non accetta più nuove richieste da parte dei
            // client connessi chiudendo tutte le connessioni attive (dovrà comunque generare il sunto delle statistiche)
            puts("ricevuto SIGINT O SIGQUIT");
            break;
        }else if(sig == SIGHUP){
            // TODO: vengono servite tutte le richieste dei client connessi al momento della ricezione del segnale, quindi il
            // server terminerà solo quando tutti i client connessi chiuderanno la connessione
            puts("ricevuto SIGHUP");
            break;
        }

    }
    return NULL;
}

/* 
    Overview: cleaning up routine. Frees allocated memory, restore process original mask.
*/
void quit(){
    client_buffer_free(clientBuffer);
    free(config_struct.socket_path);
    restoreOldMask();
}