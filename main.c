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
    Cliente** fila_pedidos;
} Garcom;

int n_clientes;
int n_garcons;
int clientes_por_garcom;
int rodadas_restantes;
int max_conversa;
int max_consumo;
int fechado = 0;

Cliente** clientes = NULL;
Garcom** garcons = NULL;

int inicializado = 0;
pthread_mutex_t mutex_status = PTHREAD_MUTEX_INITIALIZER;

void conversaComAmigos(int id) {
    printf("Cliente %d está conversando com os amigos\n", id);
    fflush(stdout);
    sleep(rand() % max_conversa);
    printf("Cliente %d terminou de conversar com os amigos\n", id);
    fflush(stdout);
}

Garcom* chamaGarcom() {
    int garcom = rand() % n_garcons;
    while (garcons[garcom]->status == 0) {
        garcom = rand() % n_garcons;
    }
    sem_wait(&garcons[garcom]->semaforoStatus);
    return garcons[garcom];
}

void fazPedido(int id, Garcom* garcom, Cliente* cliente) {
    garcom->fila_pedidos[garcom->pedidos_na_fila] = cliente;
    garcom->pedidos_na_fila++;
    printf("Cliente %d fez pedido para garçom %d\n", id, garcom->id);
    fflush(stdout);
    sem_post(&garcom->semaforoPedido);
}

void esperaPedido(int id, Garcom* garcom, Cliente* cliente) {
    printf("Cliente %d está esperando o pedido\n", id);
    fflush(stdout);
    sem_wait(&cliente->semaforo);
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
    Cliente* cliente = (Cliente*) arg;

    int id = cliente->id;

    while (rodadas_restantes > 0) {
        if (!inicializado) continue;
        conversaComAmigos(id);
        Garcom* garcom = chamaGarcom();
        fazPedido(id, garcom, cliente);
        esperaPedido(id, garcom, cliente);
        recebePedido(id, garcom);
        consomePedido(id);
    }
    pthread_exit(NULL);
    return NULL;
}

void recebeMaximoPedidos(Garcom* garcom) {
    int id  = garcom->id;
    for (int i = 0; i < clientes_por_garcom; i++) {
        printf("Garçom %d está esperando um pedido\n", id);
        fflush(stdout);
        sem_wait(&garcom->semaforoPedido);
        printf("Garçom %d recebeu um pedido\n", id);
        fflush(stdout);
    }

    pthread_mutex_lock(&mutex_status);
    garcom->status = 0;
    pthread_mutex_unlock(&mutex_status);
    
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

void entregaPedidos(Garcom* garcom) {
    int id = garcom->id;
    int pedidos_na_fila = garcom->pedidos_na_fila;
    Cliente** fila = garcom->fila_pedidos;
    for (int i = 0; i < pedidos_na_fila; i++) {
        Cliente* cliente_atual = fila[i];
        sem_post(&cliente_atual->semaforo);
        printf("Garçom %d entregou o pedido para o cliente %d\n", id, cliente_atual->id);
        fflush(stdout);
    }
    pthread_mutex_lock(&mutex_status);
    garcom->status = 1;
    pthread_mutex_unlock(&mutex_status);
    printf("Garçom %d entregou todos os pedidos\n", id);
    fflush(stdout);

    for (int i = 0; i < pedidos_na_fila; i++) {
        fila[i] = NULL;
    }
}

void* garcomThread(void* arg) {
    Garcom* garcom = (Garcom*) arg;
    
    while (rodadas_restantes > 0) {
        if (!inicializado) continue;
        recebeMaximoPedidos(garcom);
        registraPedidos(garcom->id);
        entregaPedidos(garcom);
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
    printf("Tempo máximo de consumo: %d\n\n", max_consumo);
    fflush(stdout);

    garcons = (Garcom**) malloc(sizeof(Garcom) * n_garcons);
    clientes = (Cliente**) malloc(sizeof(Cliente) * n_clientes);

    srand(time(NULL));
    pthread_mutex_init(&mutex_status, NULL);

    for (int i = 0; i < n_garcons; i++) {
        Garcom* garcom = (Garcom*) malloc(sizeof(Garcom));

        garcom->id = i;
        garcom->status = 1;
        garcom->pedidos_na_fila = 0;
        garcom->fila_pedidos = (Cliente**) malloc(sizeof(Cliente) * clientes_por_garcom);

        for (int j = 0; j < clientes_por_garcom; j++) {
            garcom->fila_pedidos[j] = NULL;
        }

        garcons[i] = garcom;

        sem_init(&garcom->semaforoStatus, 0, clientes_por_garcom);
        sem_init(&garcom->semaforoPedido, 0, 0);

        pthread_create(&garcom->thread, NULL, garcomThread, (void*) garcom);
    }

    for (int i = 0; i < n_clientes; i++) {
        Cliente* cliente = (Cliente*) malloc(sizeof(Cliente));

        cliente->id = i;

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
