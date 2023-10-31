#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

/*
Essa estrutura representa o cliente que vai frequentar o bar
Ela armzena informações como o ID, thread e o semaforo
*/
typedef struct Cliente {
    int id;
    pthread_t thread;
    sem_t aguardandoPedido;
} Cliente;

/*Essa estrutura representa o garcom que vai servir os clientes
Ela armazena informações como o ID, thread, semaforo, mutex, o status do garcom
Quantos pedidos estão na fila e o os clientes que estão aguardando serem atendidos
*/
typedef struct Garcom {
    int id;
    pthread_t thread;
    sem_t disponivel;
    sem_t pedidoRealizado;
    pthread_mutex_t mutex_fila;
    pthread_mutex_t mutex_status;
    pthread_mutex_t mutex_aguardando_atendimento;
    int status;
    int pedidos_na_fila;
    int clientes_aguardando_atendimento;
    Cliente** fila_pedidos;
} Garcom;

// Essa estrutura é usada para guardar informações sobre o bar
typedef struct StatusBar {
    int fechado;    
    int inicializado;   
    int garcons_finalizados;
    int pedidos_totais_na_fila;
    sem_t semaforo_rodada;
    pthread_mutex_t mutex_rodada;
    pthread_mutex_t mutex_bar_fechado;
    pthread_mutex_t mutex_pedidos_totais_na_fila;
    int n_garcons;
    int n_clientes;
    int max_conversa;
    int max_consumo;
    int clientes_por_garcom;
    int total_rodadas;
    Garcom** garcons;
} StatusBar;

// Estrutura que serve para passar os argumentos para a função da thread do cliente
typedef struct ArgThreadCliente {
    Cliente* cliente;
    StatusBar* bar;
} ArgThreadCliente;

// Estrutura que serve para passar os argumetos para a função da thread do garcom
typedef struct ArgThreadGarcom {
    Garcom* garcom;
    StatusBar* bar;
} ArgThreadGarcom;

// Mutex para controlar a impressão de mensagens na saída padrão
pthread_mutex_t mutex_print;

/*
Esta função serve para imprimir mensagens na saida padrao usando um mutex 
para evitar que as threads fiquem escrevendo uma sobre a outra.
*/
void printText(const char* format, ...) {
    va_list args;
    va_start(args, format);
    pthread_mutex_lock(&mutex_print);
    vprintf(format, args);
    fflush(stdout);
    pthread_mutex_unlock(&mutex_print);
    va_end(args);
}

void espera(int tempo_maximo) {
    // Método para esperar um tempo aleatório entre 0 e tempo_maximo

    // Gera um número aleatório e limita a tempo_maximo*1000 para transformar em milisegundos
    int tempo_aleatorio_em_ms = rand() % (tempo_maximo*1000);
    struct timespec req;
    req.tv_sec = tempo_aleatorio_em_ms / 1000;              // tempo base em segundos
    req.tv_nsec = (long)tempo_aleatorio_em_ms * 100000;     // tempo adicional em milisegundos

    // tv_nsec só suporta valores até 999999999ns (999ms), por esse motivo tv_sec é usado em conjunto

    // O método nanosleep espera o tempo de tv_sec + tv_nsec de req
    nanosleep(&req, NULL);
}

/*
Esta função simula o comportamento de um cliente quando ele estiver conversando com um amigo no bar
Ele passa um quantidade de tempo randomica conversando para entao fazer o pedido
*/
void conversaComAmigos(StatusBar* bar, Cliente* cliente) {
    int id = cliente->id;
    printText("Cliente %d está conversando com os amigos\n", id);
    espera(bar->max_conversa);
    printText("Cliente %d terminou de conversar com os amigos\n", id);
}

/*
Ela define se um garcom qualquer esta disponivel ou nao. Ela usa um mutex para controlar o acesso
Se o garcom estiver ocupado ele esta lockado, caso contrario esta disponivel para pegar pedidos
*/
void setStatus(Garcom* garcom, int status) {
    pthread_mutex_lock(&garcom->mutex_status);
    garcom->status = status;
    pthread_mutex_unlock(&garcom->mutex_status);
}

/*
Esta função simula um cliente chamando um garcom. O cliente vai checar se o garcom esta disponivel
Caso tenha algum garcom disponivel, ele ira chamar o garcom
Caso nao houver garcom disponivel, ele ira esperar ate um garcom estiver disponivel
E caso o bar estiver fechado, ele vai embora
*/
Garcom* chamaGarcom(StatusBar* bar, Cliente* cliente, Garcom** garcons) {
    int id = cliente->id;
    int n_garcons = bar->n_garcons;
    printText("Cliente %d está chamando um garçom\n", id);

    Garcom* garcomEscolhido = NULL;
    do {
        int garcom = rand() % n_garcons;
        garcomEscolhido = garcons[garcom];
        pthread_mutex_lock(&garcomEscolhido->mutex_status);
        printText("Cliente %d chamou o garçom %d\n", id, garcom);
        printText("(Cliente %d) Garçom %d está %s\n", id, garcom, garcomEscolhido->status == 1 ? "disponível" : "indisponível");
        if (bar->fechado) {
            printText("Cliente %d foi embora pois o bar fechou\n", id);
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

/*
Essa função serve para imprimir a fila de pedidos de um garcom
Ela basicamente vai imprimir quais clientes ja fizeram pedidos para os garcons
*/
void printFilaDePedidosDo(StatusBar* bar, Garcom* garcom, int id_cliente) {
    pthread_mutex_lock(&mutex_print);
    printf("\nCliente %d fez pedido para garçom %d\n", id_cliente, garcom->id);
    printf("Nova fila de pedidos do garçom %d:\n", garcom->id);
    for (int i = 0; i < bar->clientes_por_garcom; i++) {
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

/*
Incrementa o numero total de pedidos na fila
Usa um mutex para garantir a exclusao mutua
*/
void adicionaPedidoAoTotal(StatusBar* bar) {
    pthread_mutex_lock(&bar->mutex_pedidos_totais_na_fila);
    bar->pedidos_totais_na_fila++;
    pthread_mutex_unlock(&bar->mutex_pedidos_totais_na_fila);
}

/*Adiciona um pedido do cliente na fila de pedidos do garcom
Usa um mutex para garantir que a fila seja acessada corretamente*/
void colocaPedidoNaFilaDoGarcom(Garcom* garcom, Cliente* cliente) {
    pthread_mutex_lock(&garcom->mutex_fila);
    garcom->fila_pedidos[garcom->pedidos_na_fila] = cliente;
    garcom->pedidos_na_fila++;
    pthread_mutex_unlock(&garcom->mutex_fila);
}

/*Esta função simula um cliente fazendo um pedido para um garcom
Caso o garcom esteja ocupado ou a fila estiver cheia, ele vai tentar novamente ate que o pedido seja anotado
Esse pedido vai ser adicionado na fila do garcom*/
void fazPedido(StatusBar* bar, Cliente* cliente, Garcom** garcons) {
    int id_cliente = cliente->id;
    Garcom* garcom = chamaGarcom(bar, cliente, garcons);
    printText("Cliente %d está fazendo o pedido para o garçom %d\n", id_cliente, garcom->id);
    fflush(stdout);

    if (bar->fechado) {
        printText("Cliente %d foi embora pois o bar fechou\n", id_cliente);
        pthread_exit(NULL);
    }

    /*No raro caso onde a thread entra aqui mas o garçom já está com o máximo de pedidos
    o cliente começa o processo de fazer o pedido novamente*/
    if (garcom->pedidos_na_fila == bar->clientes_por_garcom) {
        return fazPedido(bar, cliente, garcons);
    }
    colocaPedidoNaFilaDoGarcom(garcom, cliente);   
    adicionaPedidoAoTotal(bar);
    printFilaDePedidosDo(bar, garcom, id_cliente);   
    sem_post(&garcom->pedidoRealizado);
    pthread_mutex_unlock(&garcom->mutex_fila);

}

// Ela vai simular o cliente esperando pelo seu pedido
void esperaPedido(Cliente* cliente) {
    int id = cliente->id;
    printText("Cliente %d está esperando o pedido\n", id);
    sem_wait(&cliente->aguardandoPedido);
}

// Ela vai simular o cliente recebendo o pedido
void recebePedido(Cliente* cliente) {
    int id = cliente->id;
    printText("Cliente %d recebeu o pedido\n", id);
}
/*
Ela vai simular o cliente consumindo o pedido
Que leva uma quantidade de tempo randomica, com limite superior estando no input do usuario
*/
void consomePedido(StatusBar* bar, Cliente* cliente) {
    int id = cliente->id;
    printText("Cliente %d está consumindo o pedido\n", id);
    espera(bar->max_consumo);
    printText("Cliente %d terminou de consumir o pedido\n", id);
}

/*
Esta e a função principal quando o assunto e o comportamento do cliente
Ela vai controlar o comportamento dele quando ele chegar no bar, conversar com os amigos
fazer pedido, etc
Ela vai continuar simulando o comportamento do cliente ate o bar fechar
*/
void* clienteThread(void* arg) {
    ArgThreadCliente* argCliente = (ArgThreadCliente*) arg;
    Cliente* cliente = argCliente->cliente;
    StatusBar* bar = argCliente->bar;
    Garcom** garcons = bar->garcons;
    int id = cliente->id;

    printText("Cliente %d chegou no bar\n", id);

    while (!bar->fechado) {
        if (!bar->inicializado) continue;
        conversaComAmigos(bar, cliente);
        fazPedido(bar, cliente, garcons);
        esperaPedido(cliente);
        recebePedido(cliente);
        consomePedido(bar, cliente);
    }
    printText("Cliente %d foi embora\n", id);
    pthread_exit(NULL);
}

// Ela vai retirar o cliente da fila de espera quando ele for atendido por um garcom
void removeClienteDaFilaDeEspera(Garcom* garcom) {
    pthread_mutex_lock(&garcom->mutex_aguardando_atendimento);
    garcom->clientes_aguardando_atendimento--;
    pthread_mutex_unlock(&garcom->mutex_aguardando_atendimento);
}

// Garcom vai esperar um pedido de algum cliente
int garcomAguardaPedido(StatusBar* bar, Garcom* garcom) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    // aguarda um segundo a mais que o tempo de conversa
    ts.tv_sec += bar->max_conversa+1;
    if (sem_timedwait(&garcom->pedidoRealizado, &ts)) {
        // se nenhum pedido for feito, significa que não há mais clientes querendo fazer pedidos
        printText("Garçom %d não recebeu um pedido\n", garcom->id);
        return 0;
    }
    return 1;
}

/*
Esta função serve para ver se o garcom ja chegou no limite de pedidos que ele pode receber
Caso ele tenha recebido a quantidade maxima de pedidos ele vai imprimir que o garcom ja recebeu o seu limite
*/
void recebeMaximoPedidos(StatusBar* bar, Garcom* garcom) {
    int id_garcom  = garcom->id;
    int clientes_por_garcom = bar->clientes_por_garcom;

    printText("Garçom %d agora pode receber pedidos\n", id_garcom);

    setStatus(garcom, 1);

    int recebeuMaximoPedidos = 1;

    for (int i = 0; i < clientes_por_garcom; i++) {
        printText("Garçom %d está esperando um pedido\n", id_garcom);
        sem_post(&garcom->disponivel);
        /*
        Caso todos os clientes tenham feito pedidos, não há mais como receber pedidos
        */
        if (bar->pedidos_totais_na_fila == bar->n_clientes) {
            recebeuMaximoPedidos = 0;
            break;
        }
        if (!garcomAguardaPedido(bar, garcom)) {
            i--;
            continue;
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

// Serve para printar que o garcom recebeu o pedido
void registraPedidos(Garcom* garcom) {
    printText("Garçom %d está indo para a copa para registrar os pedidos\n", garcom->id);
}

/*
Ela vai notificar que o garcom entregou o pedido para o cliente que ele atendeu
E tambem notifica ao cliente que o pedido foi entregue
*/
void entregaPedidos(StatusBar* bar, Garcom* garcom) {
    int id = garcom->id;
    int pedidos_na_fila = garcom->pedidos_na_fila;

    printText("Garçom %d está indo para a mesa entregar os pedidos\n", id);
    pthread_mutex_lock(&garcom->mutex_fila);

    for (int i = 0; i < pedidos_na_fila; i++) {
        Cliente* cliente_atual = garcom->fila_pedidos[i];
        sem_post(&cliente_atual->aguardandoPedido);
        garcom->fila_pedidos[i] = NULL;
        pthread_mutex_lock(&bar->mutex_pedidos_totais_na_fila);
        bar->pedidos_totais_na_fila--;
        pthread_mutex_unlock(&bar->mutex_pedidos_totais_na_fila);
        printText("Garçom %d entregou o pedido para o cliente %d\n", id, cliente_atual->id);
    }
    pthread_mutex_unlock(&garcom->mutex_fila);
    garcom->pedidos_na_fila = 0;
    printText("Garçom %d entregou todos os pedidos\n", id);
}

// Ela vai imprimir um texto notificando ao usuario que a rodada terminou
void printTerminoDeRodada(Garcom* garcom, int rodada) {
    pthread_mutex_lock(&mutex_print);
    printf("\n-------------------------------------------");
    printf("\nGarçom %d terminou a rodada %d\n", garcom->id, rodada-1);
    printf("-------------------------------------------\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&mutex_print);
}

// Ela vai imprimir um texto notificando ao usuario que uma nova rodada comecou
void iniciaNovaRodada(int rodada) {
    pthread_mutex_lock(&mutex_print);
    printf("\n=============================================================\n");
    printf("Rodada %d\n", rodada);
    printf("=============================================================\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&mutex_print);
}

// Ela vai fechar o bar e notificar a todos os clientes que nao sera mais possivel fazer pedidos
void fechaBar(StatusBar* bar) {
    pthread_mutex_lock(&bar->mutex_bar_fechado);
    bar->fechado = 1;
    pthread_mutex_unlock(&bar->mutex_bar_fechado);
    printText("\nTodos os garçons finalizaram o expediente. Não é possível fazer mais pedidos.\n\n");
}

// Libera todos os clientes da fila de espera do garcom para preparar para uma proxima rodada
void liberaClientesDaFilaDeEspera(Garcom* garcom) {
    pthread_mutex_lock(&garcom->mutex_aguardando_atendimento);
    for (int i = 0; i < garcom->clientes_aguardando_atendimento; i++) {
        sem_post(&garcom->disponivel);
    }
    garcom->clientes_aguardando_atendimento = 0;
    pthread_mutex_unlock(&garcom->mutex_aguardando_atendimento);
}

/*
Ela vai chamar as funcoes sobre rodada acima para finalizar uma rodada, liberar os clientes
E notifica as outras funcoes que a rodada terminou
*/
void finalizarRodada(StatusBar* bar, Garcom* garcom, int rodada) {
    printTerminoDeRodada(garcom, rodada);
    liberaClientesDaFilaDeEspera(garcom);

    pthread_mutex_lock(&bar->mutex_rodada);
    bar->garcons_finalizados++;
    pthread_mutex_unlock(&bar->mutex_rodada);
    
    int n_garcons = bar->n_garcons;
    int total_rodadas = bar->total_rodadas;

    if (bar->garcons_finalizados == n_garcons) {
        bar->garcons_finalizados = 0;
        if (rodada <= total_rodadas) {
            iniciaNovaRodada(rodada);
        } else {
            fechaBar(bar);
        }
        for (int i = 0; i < n_garcons; i++) {
            sem_post(&bar->semaforo_rodada);
        }
    }

    printText("Garçom %d não pode receber pedidos\n", garcom->id);
    sem_wait(&bar->semaforo_rodada);
}

/*Esta e a função principal que vai tratar o comportamento dos garcons
Ela vai registrar que um garcom recebeu pedidos, entregou o pedido e que terminou uma rodada
Ela vai ficar rodando ate que seja notificada que o bar fechou
*/
void* garcomThread(void* arg) {
    ArgThreadGarcom* argGarcom = (ArgThreadGarcom*) arg;
    Garcom* garcom = argGarcom->garcom;
    StatusBar* bar = argGarcom->bar;
    int rodada = 1;

    printText("Garçom %d iniciou o expediente\n", garcom->id);

    while (!bar->fechado) {
        if (!bar->inicializado) continue;
        recebeMaximoPedidos(bar, garcom);
        registraPedidos(garcom);
        entregaPedidos(bar, garcom);
        rodada++;
        finalizarRodada(bar, garcom, rodada);
    }
    printText("Garçom %d foi embora\n", garcom->id);
    pthread_exit(NULL);
}

/*
Esta função vai verificar se o argumento que foi dado pelo usuario foi um numero inteiro positivo
Caso o usuario colocou algum argumento diferente disso, o programa ira fechar imprimindo uma mensagem alertando ao usuario para so colocar numeros inteiros positivos
*/
int tratarEntrada(const char* entrada) {
    char *endptr;

    long int num = strtol(entrada, &endptr, 10);

    if ((*endptr != '\0') || num <= 0) {
        printText("Favor inserir apenas números inteiros positivos.\n");
        exit(1);
    }
    return num;
}

/*
Essa função é responsável por alocar memória para o bar e inicializar os valores padrões
*/
StatusBar* inicializaBar() {
    StatusBar* bar = (StatusBar*) malloc(sizeof(StatusBar));
    bar->fechado = 0;
    bar->inicializado = 0;
    bar->garcons_finalizados = 0;
    bar->pedidos_totais_na_fila = 0;
    bar->mutex_bar_fechado = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    bar->mutex_rodada = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    bar->mutex_pedidos_totais_na_fila = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&bar->mutex_bar_fechado, NULL);
    pthread_mutex_init(&bar->mutex_rodada, NULL);
    pthread_mutex_init(&bar->mutex_pedidos_totais_na_fila, NULL);
    sem_init(&bar->semaforo_rodada, 0, 0);
    return bar;
}

// A função main é a principal função do nosso programa
int main(int argc, char const *argv[])
{
    /*
    Aqui ela verifica os argumentos da linha de comando inserido pelo usuario
    Caso o numero de argumentos esteja incorreto ela vai printar uma mensagem de erro alertando
    ao usuario quais argumentos tem que ser dado pelo usuario
    */
    if (argc != 7) {
        printf("Uso: ./bar <n_clientes> <n_garcons> <clientes_por_garcom> <total_rodadas> <max_conversa> <max_consumo>\n");
        return 0;
    } 

    // Aqui ela vai iniciar o bar e notificar o usuario que ele foi inicializado
    printText("Iniciando bar\n");
    StatusBar* bar = inicializaBar();

    // Aqui ela vai passar os argumentos para as variaveis globais
    bar->n_clientes = tratarEntrada(argv[1]);
    bar->n_garcons = tratarEntrada(argv[2]);
    bar->clientes_por_garcom = tratarEntrada(argv[3]);
    bar->total_rodadas = tratarEntrada(argv[4]);
    bar->max_conversa = tratarEntrada(argv[5]);
    bar->max_consumo = tratarEntrada(argv[6]);
    
    printf("Número de clientes: %d\n", bar->n_clientes);
    printf("Número de garçons: %d\n", bar->n_garcons);
    printf("Máximo de clientes por garçom: %d\n", bar->clientes_por_garcom);
    printf("Quantidade de rodadas: %d\n", bar->total_rodadas);
    printf("Tempo máximo de conversa: %d\n", bar->max_conversa);
    printf("Tempo máximo de consumo: %d\n\n", bar->max_consumo);

    // Aqui ela vai alocar memória para as threads criadas dos garcons e dos clientes
    Garcom** garcons = (Garcom**) malloc(sizeof(Garcom) * bar->n_garcons);
    ArgThreadGarcom** garcons_args = (ArgThreadGarcom**) malloc(sizeof(ArgThreadGarcom) * bar->n_garcons);
    ArgThreadCliente** clientes_args = (ArgThreadCliente**) malloc(sizeof(ArgThreadCliente) * bar->n_clientes);

    bar->garcons = garcons;

    srand(time(NULL));
    pthread_mutex_init(&mutex_print, NULL);

    printf("=============================================================\n");
    printf("Rodada 1\n");
    printf("=============================================================\n\n");
    // Aqui ela vai inicializar as threads dos garcons, dando a configuração padrão para todos eles
    for (int i = 0; i < bar->n_garcons; i++) {
        ArgThreadGarcom* arg = (ArgThreadGarcom*) malloc(sizeof(ArgThreadGarcom));
        Garcom* garcom = (Garcom*) malloc(sizeof(Garcom));

        garcom->id = i;
        garcom->status = 1;     // pronto para receber pedidos
        garcom->pedidos_na_fila = 0;
        garcom->clientes_aguardando_atendimento = 0;
        garcom->fila_pedidos = (Cliente**) malloc(sizeof(Cliente) * bar->clientes_por_garcom);

        for (int j = 0; j < bar->clientes_por_garcom; j++) {
            garcom->fila_pedidos[j] = NULL;
        }

        arg->garcom = garcom;
        arg->bar = bar;
        garcons[i] = garcom;
        garcons_args[i] = arg;

        sem_init(&garcom->disponivel, 0, 0);
        sem_init(&garcom->pedidoRealizado, 0, 0);  

        garcom->mutex_fila = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
        garcom->mutex_status = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_init(&garcom->mutex_fila, NULL);
        pthread_mutex_init(&garcom->mutex_status, NULL);

        pthread_create(&garcom->thread, NULL, garcomThread, (void*) arg);
    }
    // Aqui ela vai inicializar as threads dos clientes, dando a configuração padrão para todos eles
    for (int i = 0; i < bar->n_clientes; i++) {
        ArgThreadCliente* arg = (ArgThreadCliente*) malloc(sizeof(ArgThreadCliente));
        Cliente* cliente = (Cliente*) malloc(sizeof(Cliente));

        cliente->id = i;

        arg->cliente = cliente;
        arg->bar = bar;
        clientes_args[i] = arg;

        sem_init(&cliente->aguardandoPedido, 0, 0);
        pthread_create(&cliente->thread, NULL, clienteThread, (void*) arg);
    }
    /*
    Aqui é indicado que todas as threads foram inicializadas e que o programa pode iniciar
    */
    bar->inicializado = 1;

    // Aqui ela libera os recursos que foram alocados pelos clientes
    for (int i = 0; i < bar->n_clientes; i++) {
        pthread_join(clientes_args[i]->cliente->thread, NULL);
        sem_destroy(&clientes_args[i]->cliente->aguardandoPedido);
        free(clientes_args[i]->cliente);
        free(clientes_args[i]);
    }
    free(clientes_args);

    // Aqui ela libera os recursos que foram alocados pelos clientes
    for (int i = 0; i < bar->n_garcons; i++) {
        pthread_join(garcons[i]->thread, NULL);
        sem_destroy(&garcons[i]->disponivel);
        sem_destroy(&garcons[i]->pedidoRealizado);
        free(garcons[i]->fila_pedidos);
        free(garcons[i]);
        free(garcons_args[i]);
    }
    free(garcons);
    free(garcons_args);

    // Libera os recursos do bar e imprime que o bar foi finalizado, encerrando o programa  
    free(bar);
    printf("Bar fechado\n");
    fflush(stdout);

    return 0;
}
