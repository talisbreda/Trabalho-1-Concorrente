#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

typedef struct Cliente {
    int id;
    pthread_t thread;
    int n_garcons;
    int max_conversa;
    int max_consumo;
    sem_t aguardandoPedido;
} Cliente;

typedef struct Garcom {
    int id;
    pthread_t thread;
    sem_t disponivel;
    sem_t indisponivel;
    int n_garcons;
    int clientes_por_garcom;
    int total_rodadas;
    int status;
    int pedidos_na_fila;
    Cliente** fila_pedidos;
} Garcom;


Garcom** garcons = NULL;

int fechado = 0;
int inicializado = 0;
int garcons_finalizados = 0;
pthread_mutex_t mutex_rodada = PTHREAD_MUTEX_INITIALIZER;
sem_t semaforo_rodada;
pthread_mutex_t mutex_status = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_bar_fechado = PTHREAD_MUTEX_INITIALIZER;

void conversaComAmigos(Cliente* cliente) {
    int id = cliente->id;
    printf("Cliente %d está conversando com os amigos\n", id);
    fflush(stdout);
    sleep(rand() % cliente->max_conversa);
    printf("Cliente %d terminou de conversar com os amigos\n", id);
    fflush(stdout);
}

Garcom* chamaGarcom(int n_garcons) {
    if (fechado) {
        pthread_exit(NULL);
    }
    int garcom = rand() % n_garcons;
    while (garcons[garcom] == NULL || garcons[garcom]->status != 1) {
        garcom = rand() % n_garcons;
        if (fechado) {
            pthread_exit(NULL);
        }
    }
    sem_wait(&garcons[garcom]->disponivel);
    return garcons[garcom];
}

void fazPedido(Cliente* cliente) {
    int id = cliente->id;
    Garcom* garcom = chamaGarcom(cliente->n_garcons);
    garcom->fila_pedidos[garcom->pedidos_na_fila] = cliente;
    printf("Cliente %d fez pedido para garçom %d\n", id, garcom->id);
    fflush(stdout);
    sem_post(&garcom->indisponivel);
}

void esperaPedido(Cliente* cliente) {
    int id = cliente->id;
    printf("Cliente %d está esperando o pedido\n", id);
    fflush(stdout);
    sem_wait(&cliente->aguardandoPedido);
}

void recebePedido(Cliente* cliente) {
    int id = cliente->id;
    printf("Cliente %d recebeu o pedido\n", id);
    fflush(stdout);
}

void consomePedido(Cliente* cliente) {
    int id = cliente->id;
    printf("Cliente %d está consumindo o pedido\n", id);
    fflush(stdout);
    sleep(rand() % cliente->max_consumo);
    printf("Cliente %d terminou de consumir o pedido\n", id);
    fflush(stdout);
}

void* clienteThread(void* arg) {
    Cliente* cliente = (Cliente*) arg;
    int id = cliente->id;

    printf("Cliente %d chegou no bar\n", id);
    fflush(stdout);

    while (!fechado) {
        if (!inicializado) continue;
        conversaComAmigos(cliente);
        fazPedido(cliente);
        esperaPedido(cliente);
        recebePedido(cliente);
        consomePedido(cliente);
    }
    pthread_exit(NULL);
}

void recebeMaximoPedidos(Garcom* garcom) {
    int id  = garcom->id;
    int clientes_por_garcom = garcom->clientes_por_garcom;
    printf("Garçom %d agora pode receber pedidos\n", id);
    fflush(stdout);
    pthread_mutex_lock(&mutex_status);
    garcom->status = 1;
    pthread_mutex_unlock(&mutex_status);
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
}

void entregaPedidos(Garcom* garcom) {
    int id = garcom->id;
    int pedidos_na_fila = garcom->pedidos_na_fila;
    for (int i = 0; i < pedidos_na_fila; i++) {
        Cliente* cliente_atual = garcom->fila_pedidos[i];
        printf("Cliente pointer %p\n", cliente_atual);
        sem_post(&cliente_atual->aguardandoPedido);
        garcom->fila_pedidos[i] = NULL;
        printf("Garçom %d entregou o pedido para o cliente %d\n", id, cliente_atual->id);
        fflush(stdout);
    }
    garcom->pedidos_na_fila = 0;
    printf("Garçom %d entregou todos os pedidos\n", id);
    fflush(stdout);
}

void finalizarRodada(Garcom* garcom, int rodada) {
    printf("\nGarçom %d terminou a rodada %d\n\n", garcom->id, rodada);
    fflush(stdout);
    rodada++;

    pthread_mutex_lock(&mutex_rodada);
    garcons_finalizados++;
    pthread_mutex_unlock(&mutex_rodada);
    
    int n_garcons = garcom->n_garcons;
    int total_rodadas = garcom->total_rodadas;

    if (garcons_finalizados == n_garcons) {
        garcons_finalizados = 0;
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
        for (int i = 0; i < n_garcons; i++) {
            sem_post(&semaforo_rodada);
        }
    }

    printf("Garçom %d não pode receber pedidos\n", garcom->id);
    fflush(stdout);
    sem_wait(&semaforo_rodada);
}

void* garcomThread(void* arg) {
    Garcom* garcom = (Garcom*) arg;
    int rodada = 1;

    printf("Garçom %d iniciou o expediente\n", garcom->id);
    fflush(stdout);

    while (!fechado) {
        if (!inicializado) continue;
        recebeMaximoPedidos(garcom);
        registraPedidos(garcom->id);
        entregaPedidos(garcom);
        finalizarRodada(garcom, rodada);
    }
    pthread_exit(NULL);
}

int tratarEntrada(const char* entrada) {
    char *endptr;

    long int num = strtol(entrada, &endptr, 10);

    if ((*endptr != '\0') || num <= 0) {
        free(endptr);
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
    int n_garcons = tratarEntrada(argv[2]);
    printf("Número de garçons: %d\n", n_garcons);
    fflush(stdout);
    int clientes_por_garcom = tratarEntrada(argv[3]);
    printf("Máximo de clientes por garçom: %d\n", clientes_por_garcom);
    fflush(stdout);
    int total_rodadas = tratarEntrada(argv[4]);
    printf("Quantidade de rodadas: %d\n", total_rodadas);
    fflush(stdout);
    int max_conversa = tratarEntrada(argv[5]);
    printf("Tempo máximo de conversa: %d\n", max_conversa);
    fflush(stdout);
    int max_consumo = tratarEntrada(argv[6]);
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
        garcom->status = 1;     // pronto para receber pedidos
        garcom->pedidos_na_fila = 0;
        garcom->fila_pedidos = (Cliente**) malloc(sizeof(Cliente) * clientes_por_garcom);

        for (int j = 0; j < clientes_por_garcom; j++) {
            garcom->fila_pedidos[j] = NULL;
        }

        garcom->n_garcons = n_garcons;
        garcom->clientes_por_garcom = clientes_por_garcom;
        garcom->total_rodadas = total_rodadas;
        garcons[i] = garcom;

        sem_init(&garcom->disponivel, 0, clientes_por_garcom);
        sem_init(&garcom->indisponivel, 0, 0);

        pthread_create(&garcom->thread, NULL, garcomThread, (void*) garcom);
    }

    for (int i = 0; i < n_clientes; i++) {
        Cliente* cliente = (Cliente*) malloc(sizeof(Cliente));

        cliente->id = i;
        cliente->n_garcons = n_garcons;
        cliente->max_conversa = max_conversa;
        cliente->max_consumo = max_consumo;
        clientes[i] = cliente;

        sem_init(&cliente->aguardandoPedido, 0, 0);
        pthread_create(&cliente->thread, NULL, clienteThread, (void*) cliente);
    }

    inicializado = 1;

    for (int i = 0; i < n_clientes; i++) {
        pthread_join(clientes[i]->thread, NULL);
        sem_destroy(&clientes[i]->aguardandoPedido);
        printf("Cliente %d foi embora\n", i);
        fflush(stdout);
        free(clientes[i]);
    }
    free(clientes);

    for (int i = 0; i < n_garcons; i++) {
        pthread_join(garcons[i]->thread, NULL);
        sem_destroy(&garcons[i]->disponivel);
        sem_destroy(&garcons[i]->indisponivel);
        printf("Garçom %d finalizou o expediente\n", i);
        fflush(stdout);
        free(garcons[i]->fila_pedidos);
        free(garcons[i]);
    }
    free(garcons);
    

    printf("Bar fechado\n");
    fflush(stdout);

    return 0;
}
