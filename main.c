#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

typedef struct Cliente {
    int id;
    pthread_t thread;
    sem_t semaforo;
} Cliente;

typedef struct Garcom {
    int id;
    pthread_t thread;
    sem_t semaforoStatus;
    sem_t semaforoPedido;
    int status;
    int pedidos_na_fila;
    int fila_pedidos[];
} Garcom;

typedef struct ArgThreadGarcom {
    Garcom* garcom;
    Cliente** clientes;
} ArgThreadGarcom;

typedef struct ArgThreadCliente {
    Cliente* cliente;
    Garcom** garcons;
} ArgThreadCliente;

int n_clientes;
int n_garcons;
int clientes_por_garcom;
int rodadas_restantes;
int max_conversa;
int max_consumo;
int fechado = 0;

int inicializado = 0;

void conversaComAmigos(int id) {
    printf("Cliente %d está conversando com os amigos\n", id);
    fflush(stdout);
    sleep(rand() % max_conversa);
    printf("Cliente %d terminou de conversar com os amigos\n", id);
    fflush(stdout);
}

Garcom* chamaGarcom(Garcom** garcons) {
    int garcom = rand() % sizeof(garcons);
    while (garcons[garcom]->status == 0) {
        garcom = rand() % sizeof(garcons);
    }
    sem_wait(&garcons[garcom]->semaforoStatus);
    return garcons[garcom];
}

void fazPedido(int id, Garcom* garcom) {
    garcom->fila_pedidos[garcom->pedidos_na_fila] = id;
    garcom->pedidos_na_fila++;
    sem_post(&garcom->semaforoPedido);
    printf("Cliente %d fez pedido para garçom %d\n", id, garcom->id);
    fflush(stdout);
}

void esperaPedido(int id, Garcom* garcom, sem_t semaforo) {
    printf("Cliente %d está esperando o pedido\n", id);
    fflush(stdout);
    sem_wait(&semaforo);
    printf("Cliente %d recebeu o pedido\n", id);
    fflush(stdout);
}

void recebePedido(int id, Garcom* garcom) {
    garcom->pedidos_na_fila--;
    sem_post(&garcom->semaforoStatus);
    printf("Cliente %d recebeu o pedido\n", id);
    fflush(stdout);
}

void consomePedido(int id) {
    printf("Cliente %d está consumindo o pedido\n", id);
    fflush(stdout);
    sleep(rand() % max_consumo);
    printf("Cliente %d terminou de consumir o pedido\n", id);
    fflush(stdout);
}

void* clienteThread(void* arg) {
    ArgThreadCliente* argCliente = (ArgThreadCliente*) arg;
    Cliente* cliente = argCliente->cliente;
    Garcom** garcons = argCliente->garcons;

    int id = cliente->id;
    sem_t semaforo = cliente->semaforo;

    while (rodadas_restantes > 0) {
        if (!inicializado) continue;
        conversaComAmigos(id);
        Garcom* garcom = chamaGarcom(garcons);
        fazPedido(id, garcom);
        esperaPedido(id, garcom, semaforo);
        recebePedido(id, garcom);
        consomePedido(id);
    }
    pthread_exit(NULL);
    return NULL;
}

void recebeMaximoPedidos(int id, sem_t semaforoPedido) {
    for (int i = 0; i < clientes_por_garcom; i++) {
        printf("Garçom %d está esperando um pedido\n", id);
        fflush(stdout);
        sem_wait(&semaforoPedido);
        printf("Garçom %d recebeu um pedido\n", id);
        fflush(stdout);
    }

    printf("Garçom %d recebeu o máximo de pedidos\n", id);
    fflush(stdout);
}

void registraPedidos(int id) {
    printf("Garçom %d está indo para a copa para registrar os pedidos\n", id);
    fflush(stdout);
    sleep(1);
    printf("Garçom %d está registrando os pedidos\n", id);
    fflush(stdout);
    sleep(1);
    printf("Garçom %d terminou de registrar os pedidos\n", id);
    fflush(stdout);
}

void entregaPedidos(int id, Garcom* garcom, Cliente** clientes) {
    int pedidos_na_fila = garcom->pedidos_na_fila;
    int* fila = garcom->fila_pedidos;
    for (int i = 0; i < pedidos_na_fila; i++) {
        Cliente* cliente_atual = clientes[fila[i]];
        sem_post(&cliente_atual->semaforo);
        printf("Garçom %d entregou o pedido para o cliente %d\n", id, cliente_atual->id);
        fflush(stdout);
    }

    printf("Garçom %d entregou todos os pedidos\n", id);
    fflush(stdout);

    for (int i = 0; i < pedidos_na_fila; i++) {
        fila[i] = -1;
    }
    garcom->pedidos_na_fila = 0;
}

void* garcomThread(void* arg) {
    ArgThreadGarcom* argGarcom = (ArgThreadGarcom*) arg;
    printf("Garçom %d inicializou arg\n", argGarcom->garcom->id);
    fflush(stdout);
    Garcom* garcom = argGarcom->garcom;
    printf("Garçom %d inicializou garçom\n", garcom->id);
    fflush(stdout);
    Cliente** clientes = argGarcom->clientes;
    printf("Garçom %d inicializou clientes\n", garcom->id);
    fflush(stdout);
    
    int id = garcom->id;
    sem_t semaforoPedido = garcom->semaforoPedido;

    while (rodadas_restantes > 0) {
        if (!inicializado) continue;
        recebeMaximoPedidos(id, semaforoPedido);
        registraPedidos(id);
        entregaPedidos(id, garcom, clientes);
        rodadas_restantes--;
    }
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char const *argv[])
{
    printf("Iniciando bar\n");
    fflush(stdout);
    n_clientes = atoi(argv[1]);
    printf("Número de clientes: %d\n", n_clientes);
    fflush(stdout);
    n_garcons = atoi(argv[2]);
    printf("Número de garçons: %d\n", n_garcons);
    fflush(stdout);
    clientes_por_garcom = atoi(argv[3]);
    printf("Máximo de clientes por garçom: %d\n", clientes_por_garcom);
    fflush(stdout);
    rodadas_restantes = atoi(argv[4]);
    printf("Quantidade de rodadas: %d\n", rodadas_restantes);
    fflush(stdout);
    max_conversa = atoi(argv[5]);
    printf("Tempo máximo de conversa: %d\n", max_conversa);
    fflush(stdout);
    max_consumo = atoi(argv[6]);
    printf("Tempo máximo de consumo: %d\n", max_consumo);
    fflush(stdout);

    Garcom* garcons[n_garcons];
    Cliente* clientes[n_clientes];

    for (int i = 0; i < n_garcons; i++) {
        ArgThreadGarcom* arg = (ArgThreadGarcom*) malloc(sizeof(ArgThreadGarcom));
        Garcom* garcom = (Garcom*) malloc(sizeof(Garcom));
        printf("Garçom %d criado\n", i);
        fflush(stdout);

        garcom->id = i;
        garcom->status = 1;
        garcom->pedidos_na_fila = 0;
        for (int j = 0; j < clientes_por_garcom; j++) {
            garcom->fila_pedidos[j] = -1;
        }
        printf("Garçom %d inicializado\n", i);
        fflush(stdout);

        arg->garcom = garcom;
        arg->clientes = clientes;
        garcons[i] = garcom;
        printf("Garçom %d adicionado ao vetor\n", i);
        fflush(stdout);
        
        sem_init(&garcom->semaforoStatus, 0, clientes_por_garcom);
        sem_init(&garcom->semaforoPedido, 0, 1);
        printf("Garçom %d inicializou semáforos\n", i);
        fflush(stdout);

        pthread_create(&garcom->thread, NULL, garcomThread, (void*) garcom);
        printf("Garçom %d criou thread\n", i);
        fflush(stdout);
    }

    for (int i = 0; i < n_clientes; i++) {
        ArgThreadCliente* arg = (ArgThreadCliente*) malloc(sizeof(ArgThreadCliente));
        Cliente* cliente = (Cliente*) malloc(sizeof(Cliente));

        cliente->id = i;

        arg->cliente = cliente;
        arg->garcons = garcons;
        clientes[i] = cliente;

        sem_init(&cliente->semaforo, 0, 0);
        pthread_create(&cliente->thread, NULL, clienteThread, (void*) cliente);
    }

    inicializado = 1;

    for (int i = 0; i < n_garcons; i++) {
        pthread_join(garcons[i]->thread, NULL);
        sem_destroy(&garcons[i]->semaforoStatus);
        sem_destroy(&garcons[i]->semaforoPedido);
        free(garcons[i]);
    }

    for (int i = 0; i < n_clientes; i++) {
        pthread_join(clientes[i]->thread, NULL);
        sem_destroy(&clientes[i]->semaforo);
        free(clientes[i]);
    }

    return 0;
}
