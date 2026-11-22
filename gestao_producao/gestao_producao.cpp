// gestao_producao.cpp : Este arquivo contém a função 'main'. A execução do programa começa e termina ali.
//

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h> // Por causa do strcmp
#include "../lista2.h"

HANDLE termino;
HANDLE bloqueio;
HANDLE atividade; // Usado na Etapa2


int main() // proc3
{
    // Abre os eventos que o proc1 criou
    termino = OpenEventA(EVENT_ALL_ACCESS, FALSE, "Local\\termino_gestao_producao");
    bloqueio = OpenEventA(EVENT_ALL_ACCESS, FALSE, "Local\\bloqueio_gestao_producao");

    if (termino == NULL || bloqueio == NULL) {
        printf("Eventos nao encontrados (erro %lu). Processo principal nao inicializou.\n", GetLastError());
        return 1;
    }

    // Cria o mailslot

    HANDLE mslot = CreateMailslotA(MAILSLOT_GESTAO, 0, MAILSLOT_WAIT_FOREVER, NULL);
    if (mslot == INVALID_HANDLE_VALUE) {
        printf("Falha ao criar mailslot da gestao (erro %lu).\n", GetLastError());
        exit(1);
    }

    OVERLAPPED ov;
    ZeroMemory(&ov, sizeof(ov));

    atividade = CreateEvent(NULL, TRUE, FALSE, NULL); // reset manual
    ov.hEvent = atividade;

    char msg_buffer[100];
    DWORD lidos = 0;

    ReadFile(mslot, msg_buffer, sizeof(msg_buffer) - 1, &lidos, &ov);


    // Vetor de eventos
    HANDLE eventos[3] = {termino, bloqueio, atividade};


    printf("Gestao da producao rodando normalmente.\n");

    int running = 1;
    while (running) {

        DWORD resultado = WaitForMultipleObjects(3, eventos, FALSE, INFINITE);

        // termino
        if (resultado == WAIT_OBJECT_0) {
            running = 0;
            continue;
        }

        // Bloqueio
        else if (resultado == WAIT_OBJECT_0+1) {
            printf("Gestao da producao bloqueada pelo usuario.\n");

            DWORD acordou = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);

            if (acordou == WAIT_OBJECT_0) { // Encerra
                running = 0;
                continue;
            }

            // Desbloqueio
            printf("Gestao da producao desbloqueada pelo usuario.\n");

            continue;
        }

        // Na Etapa 2 atividade é usada
        
        else if (resultado == WAIT_OBJECT_0 + 2) { // Mailslot
            GetOverlappedResult(mslot, &ov, &lidos, FALSE);
            msg_buffer[lidos] = '\0';

            if (strcmp(msg_buffer, "LIMPAR") == 0) {
                system("cls");
            }
            else {
                printf("NSEQ: %.5s OP: %.8s HORA DE INICIO: %.8s DURACAO: %.5s\n",
                    msg_buffer + 2, msg_buffer + 23, msg_buffer + 8, msg_buffer + 17); 
                // Separando a mensagem em cada trecho dela
                // COnsiderando o formato: 2/01247/07:30:00/00600/QPA-0098
            }

            // reset do handle para a leitura da proxima mensagem
            ResetEvent(atividade);
            ReadFile(mslot, msg_buffer, sizeof(msg_buffer) - 1, &lidos, &ov);
        }
        else {
            printf("Erro inesperado no WaitForMultipleObjects (WAIT_FAILED %lu).\n", GetLastError());
            running = 0;
            continue;
        }
    }

    CloseHandle(atividade);
    CloseHandle(mslot);
    CloseHandle(termino);
    CloseHandle(bloqueio);

    return 0;

}
