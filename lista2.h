#pragma once

#define TAMANHO_LISTA_2 200
#define COMPRIMENTO_MAX_MSG_2 51

#define MEMORIA "Local\\lista2_memoria"
#define VAGAS "Local\\lista2_livres"
#define MENSAGENS "Local\\lista2_ocupadas"

#define MAILSLOT_GESTAO "\\\\.\\mailslot\\correio_gestao"
#define MAILSLOT_EXIBICAO "\\\\.\\mailslot\\correio_exibicao"


struct lista_circular2 {
    int inicio;
    int final;
    char mensagens[TAMANHO_LISTA_2][COMPRIMENTO_MAX_MSG_2];
};