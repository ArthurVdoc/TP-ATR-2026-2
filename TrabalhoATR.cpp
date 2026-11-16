#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include<windows.h>
#include <conio.h>							// _getch


#define TAMANHO_LISTA 100
#define COMPRIMENTO_MAX_MSG 51


   /*
   Lista circular
   */

struct lista_circular1 {
    int inicio;
    int final;
    char mensagens[TAMANHO_LISTA][COMPRIMENTO_MAX_MSG];
};

HANDLE mutex_lista_circular; //MUTEX para lista circular

HANDLE sem_posicoes_livres; // = 100; 
// Produtor espera nesse


HANDLE sem_posicoes_ocupadas; // = 0; mas vai até 100
// Retirada espera nesse

// Eventos de termino
HANDLE termino_supervisorio; // evento que irá matar a thread supervisorio
HANDLE termino_pcp; // evento que irá matar a thread supervisorio
HANDLE termino_retirada; // evento que irá matar a thread supervisorio



// eventos de BLOQUEIO (um por tarefa)
HANDLE bloqueio_supervisorio;
HANDLE bloqueio_pcp;
HANDLE bloqueio_retirada;


// eventos de termino dos processos auxiliares
HANDLE termino_processo_gestao_producao;
HANDLE termino_processo_exibicao_processo;

// eventos de BLOQUEIO para os processos
HANDLE bloqueio_processo_gestao_producao;
HANDLE bloqueio_processo_exibicao_processo;



struct lista_circular1 LISTA_CIRCULAR;

int adicionar_elemento(struct lista_circular1* lista, char* mensagem) {
    // Semáforos cuidam de nunca haver um extravasamento da lista
    // portanto não haverá conferimento que possivelmente irá causar conflito com o semaforo aqui

    int novo_final = (lista->final + 1) % TAMANHO_LISTA;

    strcpy(&lista->mensagens[lista->final][0], mensagem);

    lista->final = novo_final;

    return 0;
}

int ler_elemento(struct lista_circular1* lista, char* destino) {
    //Semáforos cuidam de nunca haver uma retirada de lista vazia 

    int novo_inicio = (lista->inicio + 1) % TAMANHO_LISTA;


    strcpy(destino, &lista->mensagens[lista->inicio][0]);

    lista->inicio = novo_inicio;

    return 0;
}

float gerar_valor_aleatorio_float() {
    float val = (float)(rand() % (9999 + 1)); //Gerando valor aleatório até 9999 para dividir por 10
    val *= (float)0.1; // Valor limitado a 999.9 por causa da visualização
    return val;


    //Possível alteração futura: faixas diferentes para pressão volume, etc.
}

int randint(int init, int final) {
    if (final < init) {
        return randint(final, init);
    }
    else if (final == init) {
        return init;
    }

    int diff = final - init;

    return (init + rand() % (diff + 1));
}

// thread Tarefa de aguardo (Supervisório)


DWORD WINAPI supervisorio(LPVOID lpParam) {

    int tipo = 1;
    int nseq = 1;
    char saida[51];

    // Valores aleatorios
    float volume = 0.0;
    float pressao = 0.0;
    float tzona1 = 0.0;
    float tzona2 = 0.0;
    float tzona3 = 0.0;

    LARGE_INTEGER tempo;
    tempo.QuadPart = -5000000LL; // 500 ms em unidades de 100 nanosegundos

    int supervisorio_alive = 1;

    srand((unsigned int)time(NULL) + GetCurrentThreadId());

    HANDLE htimer = CreateWaitableTimer(NULL, FALSE, NULL); // Cria um timer que não é manualmente resetável
	if (htimer == NULL) {
		printf("Falha ao criar o timer do supervisorio (erro %lu).\n", GetLastError());
		return 1;
	}
	if (!SetWaitableTimer(htimer, &tempo, 500, NULL, NULL, FALSE)) {
		printf("Falha ao configurar o timer do supervisorio (erro %lu).\n", GetLastError());
		CloseHandle(htimer);
		return 1;
	}

    HANDLE eventos[3] = { termino_supervisorio, bloqueio_supervisorio, sem_posicoes_livres };

	HANDLE espera[3] = { termino_supervisorio, bloqueio_supervisorio, htimer };

    while (supervisorio_alive) {

		DWORD espera_supervisorio = WaitForMultipleObjects(3, espera, FALSE, INFINITE); // Espera por termino ou bloqueio ou timer
		if (espera_supervisorio == WAIT_OBJECT_0) { // termino_supervisorio
			supervisorio_alive = 0;
			continue;
		}
		else if (espera_supervisorio == WAIT_OBJECT_0 + 1) { // bloqueio_supervisorio
			printf("Tarefa \"supervisorio\" bloqueada pelo usuário.\n");
			DWORD acordou = WaitForMultipleObjects(2, eventos, FALSE, INFINITE); // Espera por termino ou desbloqueio
			if (acordou == WAIT_OBJECT_0) { // Encerra
				supervisorio_alive = 0;
				continue;
			}
			// Desbloqueio
            printf("Tarefa \"supervisorio\" desbloqueada pelo usuario.\n");
			continue;
		}
		else if (espera_supervisorio == WAIT_OBJECT_0 + 2) { // timer
			// Timer disparou, continue com a execução normal
		}
        else {
			printf("Erro inesperado no WaitForMultipleObjects (WAIT_FAILED).\n", GetLastError());
			supervisorio_alive = 0;
            continue;

        }


        volume = gerar_valor_aleatorio_float();
        pressao = gerar_valor_aleatorio_float();
        tzona1 = gerar_valor_aleatorio_float();
        tzona2 = gerar_valor_aleatorio_float();
        tzona3 = gerar_valor_aleatorio_float();

        // Estrutura da API Win32 que armazena a hora com milissegundos
        SYSTEMTIME tempo_atual;
        GetLocalTime(&tempo_atual);
        char data_formatada[14];
        // Formata a string extraindo os campos diretamente da estrutura SYSTEMTIME
        sprintf_s(data_formatada, sizeof(data_formatada), "%02d:%02d:%02d.%03d",
            tempo_atual.wHour, tempo_atual.wMinute, tempo_atual.wSecond, tempo_atual.wMilliseconds);


        // formatando saída
        sprintf_s(saida, sizeof(saida), "%d/%05d/%05.1f/%05.1f/%05.1f/%05.1f/%05.1f/%s",
            tipo, nseq, volume, pressao, tzona1, tzona2, tzona3, data_formatada);


        //printf(saida); Não imprimem sua saida

        DWORD result = WaitForSingleObject(sem_posicoes_livres, 0);
        // Verifica se está livre no momento

        // Mensagem caso esteja cheia
        if (result != WAIT_OBJECT_0) {
            /* Se a tentativa falhar, a lista está cheia: escreve
            o aviso na console principal e só então faz a espera bloqueante,
            combinada com o término e com o bloqueio. */

            printf("tarefa \"supervisorio\" diz: Lista circular 1 esta cheia. Esperando vaga ou bloqueio/termino...\n");

            DWORD acordou;
            do {
                acordou = WaitForMultipleObjects(3, eventos, FALSE, INFINITE); // Espera por termino ou bloqueio ou lista esvaziar

                //Esperar a lista e o término do programa:

                if (acordou == WAIT_OBJECT_0) { // termino_supervisorio
                    supervisorio_alive = 0;
                    break;
                }
                else if (acordou == WAIT_OBJECT_0 + 1) { // bloqueio_supervisorio
                    printf("Tarefa \"supervisorio\" bloqueada pelo usuário enquanto esperava por lista cheia.\n");

                    acordou = WaitForMultipleObjects(2, eventos, FALSE, INFINITE); // Espera por termino ou bloqueio ou lista esvaziar

                    if (acordou == WAIT_OBJECT_0) { // Encerra
                        supervisorio_alive = 0;
                        break;
                    }

                    // Desbloqueio

                    printf("Tarefa \"supervisorio\" desbloqueada pelo usuario.\n");

                    continue;
                }
            } while (acordou != WAIT_OBJECT_0 + 2);


            if (supervisorio_alive == 0) { // termino_supervisorio
                continue;
            }

        }

        WaitForSingleObject(mutex_lista_circular, INFINITE); //mutex lock
        adicionar_elemento(&LISTA_CIRCULAR, saida);
        ReleaseMutex(mutex_lista_circular);

        ReleaseSemaphore(sem_posicoes_ocupadas, 1, NULL);

        nseq = (nseq + 1) % (99999 + 1);

        if (nseq == 0) nseq++;
    }

	CloseHandle(htimer); // Fechar o handle do timer antes de sair da thread

    return 0;
}



// thread Tarefa de aguardo (PCP)
DWORD WINAPI pcp(LPVOID lpParam) {

    int tipo = 2;
    int nseq = 1;
    char saida[32];

    // Valores aleatorios
    int hora = 0;
    int minuto = 0;
    int segundo = 0;
    int duracao = 0;

    int pcp_alive = 1;

    srand((unsigned int)time(NULL) + GetCurrentThreadId());

	int intervalo = randint(1000, 5000); // Tempo aleatório entre 1 e 5 segundos
    LARGE_INTEGER intervalo_100ns;
    intervalo_100ns.QuadPart = -(LONGLONG)intervalo * 10000; // Convertendo para unidades de 100 nanosegundos

    HANDLE htimer = CreateWaitableTimer(NULL, FALSE, NULL); // Cria um timer que não é manualmente resetável
    if (htimer == NULL) {
        printf("Falha ao criar o timer do pcp (erro %lu).\n", GetLastError());
        return 1;
    }
    if (!SetWaitableTimer(htimer, &intervalo_100ns, 0, NULL, NULL, FALSE)) {
        printf("Falha ao configurar o timer do pcp (erro %lu).\n", GetLastError());
        CloseHandle(htimer);
        return 1;
    }


    HANDLE eventos[3] = { termino_pcp, bloqueio_pcp, sem_posicoes_livres };

	HANDLE espera[3] = { termino_pcp, bloqueio_pcp, htimer };

    while (pcp_alive) {

        DWORD espera_pcp = WaitForMultipleObjects(3, espera, FALSE, INFINITE); // Espera por termino ou bloqueio ou timer
        if (espera_pcp == WAIT_OBJECT_0) { // termino_pcp
            pcp_alive = 0;
            continue;
        }
        else if (espera_pcp == WAIT_OBJECT_0 + 1) { // bloqueio_pcp
            printf("Tarefa \"pcp\" bloqueada pelo usuário.\n");
            DWORD acordou = WaitForMultipleObjects(2, eventos, FALSE, INFINITE); // Espera por termino ou desbloqueio
            if (acordou == WAIT_OBJECT_0) { // Encerra
                pcp_alive = 0;
                continue;
            }
            // Desbloqueio
            printf("Tarefa \"pcp\" desbloqueada pelo usuario.\n");
            continue;
        }
        else if (espera_pcp == WAIT_OBJECT_0 + 2) { // timer
            // Timer disparou, reprogramação do timer e continue com a execução normal
            intervalo = randint(1000, 5000); // Tempo aleatório entre 1 e 5 segundos
            intervalo_100ns.QuadPart = -(LONGLONG)intervalo * 10000; // Convertendo para unidades de 100 nanosegundos            }
            if (!SetWaitableTimer(htimer, &intervalo_100ns, 0, NULL, NULL, FALSE)) {
                printf("Falha ao configurar o timer do pcp (erro %lu).\n", GetLastError());
                CloseHandle(htimer);
                return 1;
            }
        }
        else {
            printf("Erro inesperado no WaitForMultipleObjects (WAIT_FAILED).\n", GetLastError());
            pcp_alive = 0;
            continue;

        }


        hora = randint(0, 23);
        minuto = randint(0, 59);
        segundo = randint(0, 59);
        duracao = randint(1, 1440); // duracao em minutos: 1440 é um dia inteiro

        char letra1 = 'A' + (rand() % 26);//sorteia as letras do op
        char letra2 = 'A' + (rand() % 26);
        char letra3 = 'A' + (rand() % 26);
        int numero = rand() % 10000; //sorteia um nuemro de 4 digitos

        // formatando saída
        sprintf_s(saida, sizeof(saida), "%d/%05d/%02d:%02d:%02d/%05d/%c%c%c-%04d",
            tipo, nseq, hora, minuto, segundo, duracao, letra1, letra2, letra3, numero);
        // OP no formato AAA-NNNN: 8 caracteres alfanuméricos, como o enunciado pede.



        //printf(saida); Não imprimem sua saida

        DWORD result = WaitForSingleObject(sem_posicoes_livres, 0);
        // Verifica se está livre no momento

        // Mensagem caso esteja cheia
        if (result != WAIT_OBJECT_0) {
            /* Se a tentativa falhar, a lista está cheia: escreve
            o aviso na console principal e só então faz a espera bloqueante,
            combinada com o término e com o bloqueio. */

            printf("tarefa \"pcp\" diz: Lista circular 1 esta cheia. Esperando vaga ou bloqueio/termino...\n");

            DWORD acordou;
            do {
                acordou = WaitForMultipleObjects(3, eventos, FALSE, INFINITE); // Espera por termino ou bloqueio ou lista esvaziar

                //Esperar a lista e o término do programa:

                if (acordou == WAIT_OBJECT_0) { // termino_pcp
                    pcp_alive = 0;
                    break;
                }
                else if (acordou == WAIT_OBJECT_0 + 1) { // bloqueio_pcp
                    printf("Tarefa \"pcp\" bloqueada pelo usuário enquanto esperava por lista cheia.\n");

                    acordou = WaitForMultipleObjects(2, eventos, FALSE, INFINITE); // Espera por termino ou bloqueio ou lista esvaziar

                    if (acordou == WAIT_OBJECT_0) { // Encerra
                        pcp_alive = 0;
                        break;
                    }

                    // Desbloqueio

                    printf("Tarefa \"pcp\" desbloqueada pelo usuario.\n");

                    continue;
                }
            } while (acordou != WAIT_OBJECT_0 + 2);


            if (pcp_alive == 0) { // termino_pcp
                continue;
            }

        }

        WaitForSingleObject(mutex_lista_circular, INFINITE);
        adicionar_elemento(&LISTA_CIRCULAR, saida);
        ReleaseMutex(mutex_lista_circular);

        ReleaseSemaphore(sem_posicoes_ocupadas, 1, NULL);

        nseq = (nseq + 1) % (99999 + 1);

        if (nseq == 0) nseq++;
    }

	CloseHandle(htimer); // Fechar o handle do timer antes de sair da thread)

    return 0;
}


// thread retirada


DWORD WINAPI retirada(LPVOID lpParam) {
    // Retira elementos da LISTA e passa para os dois processos externos

    char mensagem[51];

    // bloqueada com a lista vazia.
    HANDLE eventos[3] = { termino_retirada, bloqueio_retirada, sem_posicoes_ocupadas };

    int retirada_alive = 1;
    while (retirada_alive) {

        // espera por mensagem OU por término OU por bloqueio.
        DWORD acordou = WaitForMultipleObjects(3, eventos, FALSE, INFINITE);

        if (acordou == WAIT_OBJECT_0) { // termino_retirada
            retirada_alive = 0;
            continue;
        }

        else if (acordou == WAIT_OBJECT_0 + 1) { // bloqueio_retirada

            printf("Tarefa \"retirada\" bloqueada pelo usuario.\n");

            acordou = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);
            // Espera só pelo bloqueio e termino

            if (acordou == WAIT_OBJECT_0) { // termino_retirada -> termina a tarefa
                retirada_alive = 0;
                continue;
            }

            printf("Tarefa \"retirada\" saiu do bloqueio.\n");

            continue; // Volta ao início -> desbloquado
        }

        // Caso contrário o evento era um evento de lista populada


        WaitForSingleObject(mutex_lista_circular, INFINITE);
        ler_elemento(&LISTA_CIRCULAR, mensagem);
        ReleaseMutex(mutex_lista_circular);

        ReleaseSemaphore(sem_posicoes_livres, 1, NULL);

        // Sem timer, vai retirando mensagens assim que aparecem

        // Mostra no console as mensagens retiradas da lista circular 1
        printf("%s\n", mensagem); // Adiciona quebra de linha à mensagem


        bool tipo1 = (mensagem[0] == '1');



        // (Etapa 2): esta impressão sai; se tipo1, depositar na
        // lista circular 2; senão, mandar pelo pipe/mailslot a gestao da
        // producao.



    }

    return 0;
}


int main() {
    // Inicializar lista circular com 0 elementos
    LISTA_CIRCULAR.inicio = 0;
    LISTA_CIRCULAR.final = 0;

    mutex_lista_circular = CreateMutex(NULL, FALSE, NULL); // criar mutex, nesce livre e usado apenas no processo principal
    sem_posicoes_livres = CreateSemaphore(NULL, 100, 100, NULL);
    sem_posicoes_ocupadas = CreateSemaphore(NULL, 0, 100, NULL);

    // os eventos precisam existir antes das threads, porque cada
    // thread copia os handles para o seu vetor de eventos.
    // Reset manual: o sinal fica aceso para todo mundo ver.
    termino_supervisorio = CreateEvent(NULL, TRUE, FALSE, NULL);
    termino_pcp = CreateEvent(NULL, TRUE, FALSE, NULL);
    termino_retirada = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Reset automatico: o sinal muda o estado de cada thread.
    bloqueio_supervisorio = CreateEvent(NULL, FALSE, FALSE, NULL);
    bloqueio_pcp = CreateEvent(NULL, FALSE, FALSE, NULL);
    bloqueio_retirada = CreateEvent(NULL, FALSE, FALSE, NULL);


    // Criar as três threads deste processo: PCP, Supervisório, Retirada

    HANDLE threads[3];

    // Criando as threads operárias
    threads[0] = CreateThread(NULL, 0, supervisorio, NULL, 0, NULL);
    threads[1] = CreateThread(NULL, 0, pcp, NULL, 0, NULL);
    threads[2] = CreateThread(NULL, 0, retirada, NULL, 0, NULL);



    // Criação de eventos de **processo**

    // terminos -> reset não automático
    termino_processo_exibicao_processo = CreateEventA(NULL, TRUE, FALSE, "Local\\termino_exibicao_processo");
    termino_processo_gestao_producao = CreateEventA(NULL, TRUE, FALSE, "Local\\termino_gestao_producao");

    //bloqueios -> reset automático (como nas threads)
    bloqueio_processo_exibicao_processo = CreateEventA(NULL, FALSE, FALSE, "Local\\bloqueio_exibicao_processo");
    bloqueio_processo_gestao_producao = CreateEventA(NULL, FALSE, FALSE, "Local\\bloqueio_gestao_producao");


    // Criar **nomes** dos eventos **extaprocesso** que enviarão mensagem para proc2 e proc3, 
    // pois precisarão de ser identificados pelos outros
    // Nos processos externos: abrir com OpenEventA() usando estes mesmos nomes

    // Criar os dois processos **com CreateProcess e com console nova** - antes do teclado (Proc2: Exibição de processo) - (proc3: Gestão de produção)
    // CreateProcess com a flag CREATE_NEW_CONSOLE para cada tela.

    STARTUPINFOA si_proc2, si_proc3;
    PROCESS_INFORMATION pi_exibicao_proc2, pi_exibicao_proc3;

    ZeroMemory(&si_proc2, sizeof(si_proc2));
    si_proc2.cb = sizeof(si_proc2);
    ZeroMemory(&pi_exibicao_proc2, sizeof(pi_exibicao_proc2));

    //IMPORTANTE: Verificar se os projetos são compilados no mesmo path
    if (!CreateProcessA(
        NULL,     // qual executável (caminho completo)
        (LPSTR)"exibicao_processo.exe",             // linha de comando
        NULL, NULL,       // segurança (padrão)
        FALSE,            // não herdar handles
        CREATE_NEW_CONSOLE,  // <-- a flag que dá console própria
        NULL, NULL,
        &si_proc2,
        &pi_exibicao_proc2)) {

        // teste de falha 
        // espera por um handle inválido.
        printf("Falha ao criar o processo de exibicao de processo (erro %lu).\n", GetLastError());
    }

    ZeroMemory(&si_proc3, sizeof(si_proc3));
    si_proc3.cb = sizeof(si_proc3);
    ZeroMemory(&pi_exibicao_proc3, sizeof(pi_exibicao_proc3));


    //IMPORTANTE: Verificar se os projetos são compilados no mesmo path
    if (!CreateProcessA(
        NULL,     // qual executável (caminho completo)
        (LPSTR) "gestao_producao.exe",             // linha de comando
        NULL, NULL,       // segurança (padrão)
        FALSE,            // não herdar handles
        CREATE_NEW_CONSOLE,  // <-- a flag que dá console própria
        NULL, NULL,
        &si_proc3,
        &pi_exibicao_proc3)) {
        // teste de falha 
        // espera por um handle inválido.

        printf("Falha ao criar o processo de gestao da producao (erro %lu).\n", GetLastError());
    }


    // Loop para ler teclado
        // s: alterna pausa / despausa a *leitura do supervisório* (desse processo)
        // m: alterna pausa / despausa o *leitura do PCP* (desse processo)
        // r: Alterna pausa / despausa a *tarefa de retirada* (desse processo)
        // p: Alterna pausa / despausa a *exibição de processo* (processo externo)
        // g: Alterna pausa / despausa a *gestão de produção* (processo externo)
        // 1: Limpa a tela de dados de processo (processo externo)
        // 2: Limpa a tela de acompanhamento da produção (processo externo)
        // ESC: Encerra *todas* as tarefas/processos (intra e extra processo)
    // As teclas 1 e 2 são Etapa 2 (pipe ou mailslot); aqui ficam sem efeito.

    int nTecla;
    int running = 1; // Indica que o loop principal está de pé

    do {
        nTecla = _getch();

        switch (nTecla)
        {
        case 's':
            // s: alterna pausa / despausa a *leitura do supervisório* (desse processo)
            SetEvent(bloqueio_supervisorio);
            break;

        case 'm':
            // m: alterna pausa / despausa o *leitura do PCP* (desse processo)

            SetEvent(bloqueio_pcp);

            break;

        case 'r':
            // r: Alterna pausa / despausa a *tarefa de retirada* (desse processo)

            SetEvent(bloqueio_retirada);

            break;

        case 'p':
            // p: Alterna pausa / despausa a *exibição de processo* (processo externo)

            SetEvent(bloqueio_processo_exibicao_processo);

            break;

        case 'g':
            // g: Alterna pausa / despausa a *gestão de produção* (processo externo)

            SetEvent(bloqueio_processo_gestao_producao);

            break;

        case '1':
            // 1: Limpa a tela de dados de processo (processo externo)
            // Implementação na etapa 2
            break;

        case '2':
            // 2: Limpa a tela de acompanhamento da produção (processo externo)
            // Implementação na etapa 2
            break;

        case 0x1B: // ESC
            // ESC: Encerra *todas* as tarefas/processos (intra e extra processo)
            // Manda sinal que deve ser tratado por todas as threads em paralelo com a escuta de um evento específico


            // sinalizar o término das threads
            SetEvent(termino_supervisorio);
            SetEvent(termino_pcp);
            SetEvent(termino_retirada);


            // sinalizar o término dos dois processos externos
            SetEvent(termino_processo_exibicao_processo); // Termino processo exibição de processo
            SetEvent(termino_processo_gestao_producao); // Termino processo gestão de produção

            // Esperar as threads terminar para terminar de vez
            WaitForMultipleObjects(3, threads, TRUE, INFINITE);

            // Esperar processos...
            WaitForSingleObject(pi_exibicao_proc2.hProcess, INFINITE);
            WaitForSingleObject(pi_exibicao_proc3.hProcess, INFINITE);


            // Fechando threads
            CloseHandle(threads[0]);
            CloseHandle(threads[1]);
            CloseHandle(threads[2]);

            // Fechando processos
            CloseHandle(pi_exibicao_proc2.hProcess);
            CloseHandle(pi_exibicao_proc2.hThread);
            CloseHandle(pi_exibicao_proc3.hProcess);
            CloseHandle(pi_exibicao_proc3.hThread);

            //Fechando mutex e semáforos
            CloseHandle(mutex_lista_circular);
            CloseHandle(sem_posicoes_livres);
            CloseHandle(sem_posicoes_ocupadas);

            //Fechando eventos de termino
            CloseHandle(termino_supervisorio);
            CloseHandle(termino_pcp);
            CloseHandle(termino_retirada);

            //Fechando eventos de bloqueio
            CloseHandle(bloqueio_supervisorio);
            CloseHandle(bloqueio_pcp);
            CloseHandle(bloqueio_retirada);

            //Fechando eventos dos processos externos
            CloseHandle(termino_processo_exibicao_processo);
            CloseHandle(termino_processo_gestao_producao);
            CloseHandle(bloqueio_processo_exibicao_processo);
            CloseHandle(bloqueio_processo_gestao_producao);

            // Encerrada a thread principal
            running = 0;

            break;

        default:
            continue;
        }


    } while (running);

    return 0;
}