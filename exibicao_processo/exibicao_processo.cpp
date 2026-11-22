#define _CRT_SECURE_NO_WARNINGS
// exibicao_processo.cpp : Este arquivo contém a função 'main'. A execução do programa começa e termina ali.
//

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h> // Por causa do strcmp
#include "..\lista2.h"

HANDLE termino;
HANDLE bloqueio;

HANDLE hsection;
lista_circular2* LISTA_CIRCULAR_2; // Ponteiro para a lista circular 2 na memória compartilhada

// Semáforos globais para a lista circular 2
HANDLE sem_vagas_lista_2;
HANDLE sem_mensagens_lista_2;

HANDLE mslot; // handle do mailslot
HANDLE atividade; // handle de mensagem do mailslot

int ler_elemento_lista_2(struct lista_circular2* lista, char* destino) {
    //Semáforos cuidam de nunca haver uma retirada de lista vazia 
    int novo_inicio = (lista->inicio + 1) % TAMANHO_LISTA_2;
    strcpy(destino, &lista->mensagens[lista->inicio][0]);
    lista->inicio = novo_inicio;
    return 0;
}

int main() // proc2
{
    // Abre os eventos que o proc1 criou
    termino = OpenEventA(EVENT_ALL_ACCESS, FALSE, "Local\\termino_exibicao_processo");
    bloqueio = OpenEventA(EVENT_ALL_ACCESS, FALSE, "Local\\bloqueio_exibicao_processo");

    if (termino == NULL || bloqueio == NULL) {
        printf("Eventos nao encontrados (erro %lu). Processo principal nao inicializou.\n", GetLastError());
        return 1;
    }

	hsection = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, MEMORIA);
	if (hsection == NULL) {
		printf("Falha ao abrir a secao de memoria compartilhada (erro %lu).\n", GetLastError());
		return 1;
	}

    LISTA_CIRCULAR_2 = (struct lista_circular2*)MapViewOfFile(hsection, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (LISTA_CIRCULAR_2 == NULL) {
		printf("Falha ao mapear a secao de memoria compartilhada (erro %lu).\n", GetLastError());
		CloseHandle(hsection);
		return 1;
	}

	sem_vagas_lista_2 = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, VAGAS);
	sem_mensagens_lista_2 = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, MENSAGENS);
	if (sem_mensagens_lista_2 == NULL || sem_vagas_lista_2 == NULL) {
		printf("Falha ao abrir os semaforos para a lista 2 (erro %lu)\n", GetLastError());
        return 1;
	}

    // Criar mailslot

    mslot = CreateMailslotA(MAILSLOT_EXIBICAO, 0, MAILSLOT_WAIT_FOREVER, NULL);

    if (mslot == INVALID_HANDLE_VALUE) {
        printf("Falha ao criar mailslot da exibicao (erro: %lu).\n", GetLastError());
        exit(1);
    }

    OVERLAPPED ov;
    ZeroMemory(&ov, sizeof(ov));

    atividade = CreateEvent(NULL, TRUE, FALSE, NULL); // reset manual
    ov.hEvent = atividade;

    char msg_buffer[100];
    DWORD lidos = 0;

    ReadFile(mslot, msg_buffer, sizeof(msg_buffer) - 1, &lidos, &ov);


    HANDLE eventos[4] = { termino, bloqueio, sem_mensagens_lista_2, atividade };

    printf("Exibicao de processo rodando normalmente.\n");

    int running = 1;
    while (running) {


        DWORD resultado = WaitForMultipleObjects(4, eventos, FALSE, INFINITE);
        
        // termino
        if (resultado == WAIT_OBJECT_0) {
            running = 0;
            continue;
        }

        // Bloqueio
        else if (resultado == WAIT_OBJECT_0 + 1) {
            printf("Exibicao de processo bloqueada pelo usuario.\n");

            DWORD acordou = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);

            if (acordou == WAIT_OBJECT_0) { // Encerra
                running = 0;
                continue;
            }

            printf("Exibicao de processo desbloqueada pelo usuario.\n");

            continue;
        }
        // Na Etapa 2 atividade é usada
        //placeholder
		else if (resultado == WAIT_OBJECT_0 + 2) {
			// Retira elemento da lista circular 2
			char mensagem[COMPRIMENTO_MAX_MSG_2];
			ler_elemento_lista_2(LISTA_CIRCULAR_2, mensagem);
			if (!ReleaseSemaphore(sem_vagas_lista_2, 1, NULL)) {
				printf("Falha ao liberar semaforo de vagas para a lista 2 (erro %lu).\n", GetLastError());
			}
			// Exibe a mensagem
			printf("NSEQ: %.5s TZ1: %.5s TZ2: %.5s TZ3: %.5s VOL: %.5s PRES: %.5s HORA: %.12s\n", 
                mensagem+2, mensagem+20, mensagem+26, mensagem+32, mensagem+8, mensagem+14, mensagem+38);
		} 

        // mailslot: comando de limpeza
        else if (resultado == WAIT_OBJECT_0 + 3) { 
            GetOverlappedResult(mslot, &ov, &lidos, FALSE);
            msg_buffer[lidos] = '\0';

            if (strcmp(msg_buffer, "LIMPAR") == 0) {
                system("cls");
            }

            ResetEvent(atividade);
            ReadFile(mslot, msg_buffer, sizeof(msg_buffer) - 1, &lidos, &ov);
        }
		else {
			printf("Erro inesperado no WaitForMultipleObjects (WAIT_FAILED %lu).\n", GetLastError());
			running = 0;
			continue;
		}
    }

	UnmapViewOfFile(LISTA_CIRCULAR_2);//desfaz o mapeamento da memória compartilhada
	CloseHandle(hsection); // Fecha o handle da seção de memória compartilhada
	CloseHandle(sem_vagas_lista_2);
	CloseHandle(sem_mensagens_lista_2);

    CloseHandle(atividade);
    CloseHandle(mslot);

    CloseHandle(termino);
    CloseHandle(bloqueio);

    return 0;

}
