#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

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
    sem_t pedidoRealizado;
    pthread_mutex_t mutex_fila;
    pthread_mutex_t mutex_status;
    pthread_mutex_t mutex_aguardando_atendimento;
    int max_conversa;
    int n_garcons;
    int clientes_por_garcom;
    int total_rodadas;
    int status;
    int pedidos_na_fila;
    int clientes_aguardando_atendimento;
    Cliente** fila_pedidos;
} Garcom;


Garcom** garcons = NULL;

int fechado = 0;
int inicializado = 0;
int garcons_finalizados = 0;
sem_t semaforo_rodada;
pthread_mutex_t mutex_rodada = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_bar_fechado = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_print = PTHREAD_MUTEX_INITIALIZER;

void printText(const char* format, ...) {
    va_list args;
    va_start(args, format);
    pthread_mutex_lock(&mutex_print);
    vprintf(format, args);
    fflush(stdout);
    pthread_mutex_unlock(&mutex_print);
    va_end(args);
}

void conversaComAmigos(Cliente* cliente) {
    int id = cliente->id;
    printText("Cliente %d está conversando com os amigos\n", id);
    sleep(rand() % cliente->max_conversa);
    printText("Cliente %d terminou de conversar com os amigos\n", id);
}

void setStatus(Garcom* garcom, int status) {
    pthread_mutex_lock(&garcom->mutex_status);
    garcom->status = status;
    pthread_mutex_unlock(&garcom->mutex_status);
}

Garcom* chamaGarcom(Cliente* cliente) {
    int id = cliente->id;
    int n_garcons = cliente->n_garcons;
    printText("Cliente %d está chamando um garçom\n", id);

    Garcom* garcomEscolhido = NULL;
    do {
        int garcom = rand() % n_garcons;
        garcomEscolhido = garcons[garcom];
        pthread_mutex_lock(&garcomEscolhido->mutex_status);
        printText("Cliente %d chamou o garçom %d\n", id, garcom);
        printText("(Cliente %d) Garçom %d está %s\n", id, garcom, garcomEscolhido->status == 1 ? "disponível" : "indisponível");
        if (fechado) {
            printText("Cliente %d foi embora pois não há mais garçons\n", id);
            pthread_mutex_unlock(&garcomEscolhido->mutex_status);
            pthread_exit(NULL);
        }
        if (garcomEscolhido->status == 0) {
            pthread_mutex_unlock(&garcomEscolhido->mutex_status);
            continue;
        }
        garcomEscolhido->clientes_aguardando_atendimento++;
        pthread_mutex_unlock(&garcomEscolhido->mutex_status);
        sem_wait(&garcomEscolhido->disponivel); 
    } while (garcomEscolhido->status != 1);

    return garcomEscolhido;
}

void printFilaDePedidosDo(Garcom* garcom, int id_cliente) {
    pthread_mutex_lock(&mutex_print);
    printf("\nCliente %d fez pedido para garçom %d\n", id_cliente, garcom->id);
    printf("Nova fila de pedidos do garçom %d:\n", garcom->id);
    for (int i = 0; i < garcom->clientes_por_garcom; i++) {
        if (garcom->fila_pedidos[i] == NULL) {
            printf("%d: Vazio\n", i);
            continue;
        }
        printf("%d: Cliente %d\n", i, garcom->fila_pedidos[i]->id);
    }
    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&mutex_print);
}

void fazPedido(Cliente* cliente) {
    int id_cliente = cliente->id;
    Garcom* garcom = chamaGarcom(cliente);
    printText("Cliente %d está fazendo o pedido para o garçom %d\n", id_cliente, garcom->id);
    fflush(stdout);

    pthread_mutex_lock(&garcom->mutex_fila);
    if (fechado) {
        printText("Cliente %d foi embora pois não há mais garçons\n", id_cliente);
        pthread_mutex_unlock(&garcom->mutex_fila);
        pthread_exit(NULL);
    }
    // No raro caso onde a thread entra aqui mas o garçom já está com o máximo de pedidos
    // o cliente começa o processo de fazer o pedido novamente
    if (garcom->pedidos_na_fila == garcom->clientes_por_garcom) {
        pthread_mutex_unlock(&garcom->mutex_fila);
        return fazPedido(cliente);
    }
    garcom->fila_pedidos[garcom->pedidos_na_fila] = cliente;
    garcom->pedidos_na_fila++;
    printFilaDePedidosDo(garcom, id_cliente);   
    sem_post(&garcom->pedidoRealizado);
    pthread_mutex_unlock(&garcom->mutex_fila);

}

void esperaPedido(Cliente* cliente) {
    int id = cliente->id;
    printText("Cliente %d está esperando o pedido\n", id);
    sem_wait(&cliente->aguardandoPedido);
}

void recebePedido(Cliente* cliente) {
    int id = cliente->id;
    printText("Cliente %d recebeu o pedido\n", id);
}

void consomePedido(Cliente* cliente) {
    int id = cliente->id;
    printText("Cliente %d está consumindo o pedido\n", id);
    sleep(rand() % cliente->max_consumo);
    printText("Cliente %d terminou de consumir o pedido\n", id);
}

void* clienteThread(void* arg) {
    Cliente* cliente = (Cliente*) arg;
    int id = cliente->id;

    printText("Cliente %d chegou no bar\n", id);

    while (!fechado) {
        if (!inicializado) continue;
        conversaComAmigos(cliente);
        fazPedido(cliente);
        esperaPedido(cliente);
        recebePedido(cliente);
        consomePedido(cliente);
    }
    printText("Cliente %d foi embora\n", id);
    pthread_exit(NULL);
}

void removeClienteDaFilaDeEspera(Garcom* garcom) {
    pthread_mutex_lock(&garcom->mutex_aguardando_atendimento);
    garcom->clientes_aguardando_atendimento--;
    pthread_mutex_unlock(&garcom->mutex_aguardando_atendimento);
}

int garcomAguardaPedido(Garcom* garcom) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    // aguarda um segundo a mais que o tempo de conversa
    ts.tv_sec += garcom->max_conversa+1;
    if (sem_timedwait(&garcom->pedidoRealizado, &ts)) {
        // se nenhum pedido for feito, significa que não há mais clientes querendo fazer pedidos
        printText("Garçom %d não recebeu um pedido\n", garcom->id);
        return 0;
    }
    return 1;
}

void recebeMaximoPedidos(Garcom* garcom) {
    int id_garcom  = garcom->id;
    int clientes_por_garcom = garcom->clientes_por_garcom;

    printText("Garçom %d agora pode receber pedidos\n", id_garcom);

    setStatus(garcom, 1);

    int recebeuMaximoPedidos = 1;

    for (int i = 0; i < clientes_por_garcom; i++) {
        printText("Garçom %d está esperando um pedido\n", id_garcom);
        sem_post(&garcom->disponivel);
        /*
        Se o garçom não recebeu um pedido por um certo período de tempo, 
        não há mais clientes tentando fazer pedidos
        */
        if (!garcomAguardaPedido(garcom)) {
            recebeuMaximoPedidos = 0;
            break;
        };
        removeClienteDaFilaDeEspera(garcom);
        printText("Garçom %d recebeu um pedido\n", id_garcom);
    }

    setStatus(garcom, 0);

    if (recebeuMaximoPedidos) {
        printText("Garçom %d recebeu o máximo de pedidos\n", id_garcom);
    } else {
        printText("Garçom %d não recebeu o máximo de pedidos pois não há mais clientes querendo pedir\n", id_garcom);
    }
}

void registraPedidos(Garcom* garcom) {
    printText("Garçom %d está indo para a copa para registrar os pedidos\n", garcom->id);
}

void entregaPedidos(Garcom* garcom) {
    int id = garcom->id;
    int pedidos_na_fila = garcom->pedidos_na_fila;

    printText("Garçom %d está indo para a mesa entregar os pedidos\n", id);
    pthread_mutex_lock(&garcom->mutex_fila);

    for (int i = 0; i < pedidos_na_fila; i++) {
        Cliente* cliente_atual = garcom->fila_pedidos[i];
        sem_post(&cliente_atual->aguardandoPedido);
        garcom->fila_pedidos[i] = NULL;
        printText("Garçom %d entregou o pedido para o cliente %d\n", id, cliente_atual->id);
    }
    pthread_mutex_unlock(&garcom->mutex_fila);
    garcom->pedidos_na_fila = 0;
    printText("Garçom %d entregou todos os pedidos\n", id);
}

void printTerminoDeRodada(Garcom* garcom, int rodada) {
    pthread_mutex_lock(&mutex_print);
    printf("\n-------------------------------------------");
    printf("\nGarçom %d terminou a rodada %d\n", garcom->id, rodada-1);
    printf("-------------------------------------------\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&mutex_print);
}

void iniciaNovaRodada(int rodada) {
    pthread_mutex_lock(&mutex_print);
    printf("\n=============================================================\n");
    printf("Rodada %d\n", rodada);
    printf("=============================================================\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&mutex_print);
}

void fechaBar() {
    pthread_mutex_lock(&mutex_bar_fechado);
    fechado = 1;
    pthread_mutex_unlock(&mutex_bar_fechado);
    printText("\nTodos os garçons finalizaram o expediente. Não é possível fazer mais pedidos.\n\n");
}

void liberaClientesDaFilaDeEspera(Garcom* garcom) {
    pthread_mutex_lock(&garcom->mutex_aguardando_atendimento);
    for (int i = 0; i < garcom->clientes_aguardando_atendimento; i++) {
        sem_post(&garcom->disponivel);
    }
    garcom->clientes_aguardando_atendimento = 0;
    pthread_mutex_unlock(&garcom->mutex_aguardando_atendimento);
}

void finalizarRodada(Garcom* garcom, int rodada) {
    printTerminoDeRodada(garcom, rodada);
    liberaClientesDaFilaDeEspera(garcom);

    pthread_mutex_lock(&mutex_rodada);
    garcons_finalizados++;
    pthread_mutex_unlock(&mutex_rodada);
    
    int n_garcons = garcom->n_garcons;
    int total_rodadas = garcom->total_rodadas;

    if (garcons_finalizados == n_garcons) {
        garcons_finalizados = 0;
        if (rodada <= total_rodadas) {
            iniciaNovaRodada(rodada);
        } else {
            fechaBar();
        }
        for (int i = 0; i < n_garcons; i++) {
            sem_post(&semaforo_rodada);
        }
    }

    printText("Garçom %d não pode receber pedidos\n", garcom->id);
    sem_wait(&semaforo_rodada);
}

void* garcomThread(void* arg) {
    Garcom* garcom = (Garcom*) arg;
    int rodada = 1;

    printText("Garçom %d iniciou o expediente\n", garcom->id);

    while (!fechado) {
        if (!inicializado) continue;
        recebeMaximoPedidos(garcom);
        registraPedidos(garcom);
        entregaPedidos(garcom);
        rodada++;
        finalizarRodada(garcom, rodada);
    }
    printText("Garçom %d foi embora\n", garcom->id);
    pthread_exit(NULL);
}

int tratarEntrada(const char* entrada) {
    char *endptr;

    long int num = strtol(entrada, &endptr, 10);

    if ((*endptr != '\0') || num <= 0) {
        free(endptr);
        printText("Favor inserir apenas números inteiros positivos.\n");
        exit(1);
    }
    return num;
}

int main(int argc, char const *argv[])
{
    printText("Iniciando bar\n");

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
    pthread_mutex_init(&mutex_rodada, NULL);
    pthread_mutex_init(&mutex_bar_fechado, NULL);
    pthread_mutex_init(&mutex_print, NULL);

    sem_init(&semaforo_rodada, 0, 0);

    printf("=============================================================\n");
    printf("Rodada 1\n");
    printf("=============================================================\n\n");

    for (int i = 0; i < n_garcons; i++) {
        Garcom* garcom = (Garcom*) malloc(sizeof(Garcom));

        garcom->id = i;
        garcom->status = 1;     // pronto para receber pedidos
        garcom->pedidos_na_fila = 0;
        garcom->clientes_aguardando_atendimento = 0;
        garcom->fila_pedidos = (Cliente**) malloc(sizeof(Cliente) * clientes_por_garcom);

        for (int j = 0; j < clientes_por_garcom; j++) {
            garcom->fila_pedidos[j] = NULL;
        }

        garcom->n_garcons = n_garcons;
        garcom->clientes_por_garcom = clientes_por_garcom;
        garcom->total_rodadas = total_rodadas;
        garcom->max_conversa = max_conversa;
        garcons[i] = garcom;

        sem_init(&garcom->disponivel, 0, 0);
        sem_init(&garcom->pedidoRealizado, 0, 0);  

        garcom->mutex_fila = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
        garcom->mutex_status = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_init(&garcom->mutex_fila, NULL);
        pthread_mutex_init(&garcom->mutex_status, NULL);

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
        fflush(stdout);
        free(clientes[i]);
    }
    free(clientes);

    for (int i = 0; i < n_garcons; i++) {
        pthread_join(garcons[i]->thread, NULL);
        sem_destroy(&garcons[i]->disponivel);
        sem_destroy(&garcons[i]->pedidoRealizado);
        free(garcons[i]->fila_pedidos);
        free(garcons[i]);
    }
    free(garcons);
    

    printf("Bar fechado\n");
    fflush(stdout);

    return 0;
}
