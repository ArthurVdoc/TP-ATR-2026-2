// gestao_producao.cpp : Este arquivo contém a função 'main'. A execução do programa começa e termina ali.
//

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

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

    HANDLE eventos[3] = {termino, bloqueio, atividade};


    printf("Gestao da producao rodando normalmente.\n");

    int running = 1;
    while (running) {

        DWORD resultado = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);

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
        //placeholder
    }

    CloseHandle(termino);
    CloseHandle(bloqueio);

    return 0;

}
