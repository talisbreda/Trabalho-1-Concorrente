#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

typedef struct Cliente {
    int id;
    pthread_t thread;
    sem_t aguardandoPedido;
} Cliente;

typedef struct Garcom {
    int id;
    pthread_t thread;
    sem_t disponivel;
    sem_t indisponivel;
    int status;
    int pedidos_na_fila;
    Cliente** fila_pedidos;
} Garcom;

int n_garcons;
int clientes_por_garcom;
int total_rodadas;
int max_conversa;
int max_consumo;
int fechado = 0;

Garcom** garcons = NULL;

int inicializado = 0;
int garcons_finalizados = 0;
pthread_mutex_t mutex_rodada = PTHREAD_MUTEX_INITIALIZER;
sem_t semaforo_rodada;
pthread_mutex_t mutex_status = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_bar_fechado = PTHREAD_MUTEX_INITIALIZER;

void conversaComAmigos(int id) {
    printf("Cliente %d está conversando com os amigos\n", id);
    fflush(stdout);
    sleep(rand() % max_conversa);
    printf("Cliente %d terminou de conversar com os amigos\n", id);
    fflush(stdout);
}

Garcom* chamaGarcom() {
    int garcom = rand() % n_garcons;
    if (fechado) {
        pthread_exit(NULL);
        return NULL;
    }
    while (garcons[garcom] == NULL || garcons[garcom]->status != 1) {
        garcom = rand() % n_garcons;
        if (fechado) break;
    }
    if (garcons[garcom] == NULL) {
        pthread_exit(NULL);
        return NULL;
    }
    sem_wait(&garcons[garcom]->disponivel);
    return garcons[garcom];
}

void fazPedido(int id, Garcom* garcom, Cliente* cliente) {
    garcom->fila_pedidos[garcom->pedidos_na_fila] = cliente;
    printf("Cliente %d fez pedido para garçom %d\n", id, garcom->id);
    fflush(stdout);
    sem_post(&garcom->indisponivel);
}

void esperaPedido(int id, Garcom* garcom, Cliente* cliente) {
    printf("Cliente %d está esperando o pedido\n", id);
    fflush(stdout);
    sem_wait(&cliente->aguardandoPedido);
}

void recebePedido(int id, Garcom* garcom) {
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

    while (!fechado) {
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
    printf("Garçom %d agora pode receber pedidos\n", id);
    fflush(stdout);
    for (int i = 0; i < clientes_por_garcom; i++) {
        printf("Garçom %d está esperando um pedido\n", id);
        fflush(stdout);
        sem_post(&garcom->disponivel);
        sem_wait(&garcom->indisponivel);
        garcom->pedidos_na_fila++;
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
    printf("Size: %d\n", pedidos_na_fila);
    for (int i = 0; i < pedidos_na_fila; i++) {
        Cliente* cliente_atual = fila[i];
        sem_post(&cliente_atual->aguardandoPedido);
        garcom->fila_pedidos[i] = NULL;
        garcom->pedidos_na_fila--;
        printf("Garçom %d entregou o pedido para o cliente %d\n", id, cliente_atual->id);
        fflush(stdout);
    }
    printf("Garçom %d entregou todos os pedidos\n", id);
    fflush(stdout);
}

void finalizarRodada(int rodada) {
    pthread_mutex_lock(&mutex_rodada);
    garcons_finalizados++;
    pthread_mutex_unlock(&mutex_rodada);
    if (garcons_finalizados == n_garcons) {
        garcons_finalizados = 0;
        for (int i = 0; i < n_garcons; i++) {
            sem_post(&semaforo_rodada);
        }
        if (rodada <= total_rodadas) {
            printf("\n-------------------------------------------------------------\n");
            printf("Rodada %d\n", rodada);
            printf("-------------------------------------------------------------\n\n");
        } else {
            pthread_mutex_lock(&mutex_bar_fechado);
            fechado = 1;
            pthread_mutex_unlock(&mutex_bar_fechado);
            printf("\nTodos os garçons finalizaram o expediente. Não é possível fazer mais pedidos.\n\n");
        }
    }
}

void* garcomThread(void* arg) {
    Garcom* garcom = (Garcom*) arg;
    int rodada = 1;

    while (!fechado) {
        if (!inicializado) continue;
        pthread_mutex_lock(&mutex_status);
        garcom->status = 1;
        pthread_mutex_unlock(&mutex_status);
        recebeMaximoPedidos(garcom);
        registraPedidos(garcom->id);
        entregaPedidos(garcom);
        printf("\nGarçom %d terminou a rodada %d\n\n", garcom->id, rodada);
        fflush(stdout);
        rodada++;
        finalizarRodada(rodada);
        printf("Garçom %d ainda não pode receber pedidos\n", garcom->id);
        fflush(stdout);
        sem_wait(&semaforo_rodada);
    }
    pthread_exit(NULL);
    return NULL;
}

int tratarEntrada(const char* entrada) {
    char *endptr;

    long int num = strtol(entrada, &endptr, 10);

    if ((*endptr != '\0') || num <= 0) {
        printf("Favor inserir apenas números inteiros positivos.\n");
        exit(1);
    }
    return num;
}

int main(int argc, char const *argv[])
{
    printf("Iniciando bar\n");

    if (argc != 7) {
        printf("Uso: ./bar <n_clientes> <n_garcons> <clientes_por_garcom> <total_rodadas> <max_conversa> <max_consumo>\n");
        return 0;
    } 

    int n_clientes = tratarEntrada(argv[1]);
    printf("Número de clientes: %d\n", n_clientes);
    fflush(stdout);
    n_garcons = tratarEntrada(argv[2]);
    printf("Número de garçons: %d\n", n_garcons);
    fflush(stdout);
    clientes_por_garcom = tratarEntrada(argv[3]);
    printf("Máximo de clientes por garçom: %d\n", clientes_por_garcom);
    fflush(stdout);
    total_rodadas = tratarEntrada(argv[4]);
    printf("Quantidade de rodadas: %d\n", total_rodadas);
    fflush(stdout);
    max_conversa = tratarEntrada(argv[5]);
    printf("Tempo máximo de conversa: %d\n", max_conversa);
    fflush(stdout);
    max_consumo = tratarEntrada(argv[6]);
    printf("Tempo máximo de consumo: %d\n\n", max_consumo);
    fflush(stdout);

    garcons = (Garcom**) malloc(sizeof(Garcom) * n_garcons);
    Cliente** clientes = (Cliente**) malloc(sizeof(Cliente) * n_clientes);

    srand(time(NULL));
    pthread_mutex_init(&mutex_status, NULL);
    pthread_mutex_init(&mutex_rodada, NULL);
    pthread_mutex_init(&mutex_bar_fechado, NULL);

    sem_init(&semaforo_rodada, 0, 0);

    printf("-------------------------------------------------------------\n");
    printf("Rodada 1\n");
    printf("-------------------------------------------------------------\n\n");

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

        sem_init(&garcom->disponivel, 0, clientes_por_garcom);
        sem_init(&garcom->indisponivel, 0, 0);

        pthread_create(&garcom->thread, NULL, garcomThread, (void*) garcom);
    }

    for (int i = 0; i < n_clientes; i++) {
        Cliente* cliente = (Cliente*) malloc(sizeof(Cliente));

        cliente->id = i;
        clientes[i] = cliente;

        sem_init(&cliente->aguardandoPedido, 0, 0);
        pthread_create(&cliente->thread, NULL, clienteThread, (void*) cliente);
    }

    inicializado = 1;

    for (int i = 0; i < n_clientes; i++) {
        pthread_join(clientes[i]->thread, NULL);
        printf("Semáforo do cliente %d será destruído\n", i);
        fflush(stdout);
        sem_destroy(&clientes[i]->aguardandoPedido);
        printf("Semáforo do cliente %d destruído\n", i);
        fflush(stdout);
        printf("Cliente %d foi embora\n", i);
        fflush(stdout);
        free(clientes[i]);
    }

    for (int i = 0; i < n_garcons; i++) {
        pthread_join(garcons[i]->thread, NULL);
        printf("Semáforos do garçom serão destruídos\n");
        fflush(stdout);
        sem_destroy(&garcons[i]->disponivel);
        printf("Semáforo disponível do garçom %d destruído\n", i);
        fflush(stdout);
        sem_destroy(&garcons[i]->indisponivel);
        printf("Semáforo indisponível do garçom %d destruído\n", i);
        fflush(stdout);
        printf("Garçom %d finalizou o expediente\n", i);
        fflush(stdout);
        free(garcons[i]);
    }
    

    printf("Bar fechado\n");
    fflush(stdout);

    return 0;
}
