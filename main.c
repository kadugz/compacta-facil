#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "compactafacil.h"

static void lerLinha(char *buffer, int tamanho) {
    if (fgets(buffer, tamanho, stdin)) {
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
            buffer[--len] = '\0';
    } else {
        buffer[0] = '\0';
    }
}

static int arquivoExiste(const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

/* pede confirmacao antes de sobrescrever um arquivo que ja existe */
static int podeGravar(const char *caminho) {
    char resposta[8];
    if (!arquivoExiste(caminho))
        return 1;
    printf("O arquivo \"%s\" ja existe. Sobrescrever? (s/n): ", caminho);
    lerLinha(resposta, sizeof(resposta));
    return (resposta[0] == 's' || resposta[0] == 'S');
}

static void mostrarEstatisticas(const Estatisticas *stats) {
    printf("Tamanho original: %" PRIu64 " bytes\n", stats->tamanhoOriginal);
    printf("Tamanho comprimido: %" PRIu64 " bytes\n", stats->tamanhoComprimido);
    printf("Modo: %s\n", stats->modoArmazenado
                              ? "armazenado (sem codificar, cabecalho mantem o arquivo pequeno)"
                              : "huffman");

    if (stats->tamanhoOriginal == 0) {
        printf("Razao de compressao: nao se aplica (arquivo vazio)\n");
    } else {
        double razao = (double)stats->tamanhoComprimido / (double)stats->tamanhoOriginal;
        printf("Razao de compressao: %.4f\n", razao);
        printf("Economia: %.2f%%\n", (1.0 - razao) * 100.0);
    }

    printf("Tempo de processamento: %.4f s\n", stats->tempoSegundos);
}

static void comprimir(void) {
    char origem[512], destino[512];
    Estatisticas stats;
    int r;

    printf("Caminho do arquivo a comprimir: ");
    lerLinha(origem, sizeof(origem));
    if (origem[0] == '\0') {
        printf("Erro: caminho vazio.\n");
        return;
    }

    snprintf(destino, sizeof(destino), "%s.cfz", origem);
    if (!podeGravar(destino)) {
        printf("Operacao cancelada.\n");
        return;
    }

    r = comprimirArquivo(origem, destino, &stats);
    if (r == COMPACTA_ERRO_ORIGEM) {
        printf("Erro: nao foi possivel abrir \"%s\". Verifique se o caminho esta correto.\n", origem);
        return;
    }
    if (r == COMPACTA_ERRO_DESTINO) {
        printf("Erro: nao foi possivel criar o arquivo comprimido.\n");
        return;
    }

    printf("Arquivo comprimido criado em: %s\n", destino);
    mostrarEstatisticas(&stats);
}

static void descomprimir(void) {
    char origem[512], destino[512];
    Estatisticas stats;
    int r;

    printf("Caminho do arquivo comprimido (.cfz): ");
    lerLinha(origem, sizeof(origem));
    if (origem[0] == '\0') {
        printf("Erro: caminho vazio.\n");
        return;
    }
    if (strlen(origem) < 4 || strcmp(origem + strlen(origem) - 4, ".cfz") != 0)
        printf("Aviso: o arquivo nao tem extensao .cfz, mas a leitura sera tentada mesmo assim.\n");

    printf("Caminho para salvar o arquivo restaurado: ");
    lerLinha(destino, sizeof(destino));
    if (destino[0] == '\0') {
        printf("Erro: caminho vazio.\n");
        return;
    }
    if (strcmp(origem, destino) == 0) {
        printf("Erro: o arquivo de saida nao pode ser igual ao arquivo comprimido.\n");
        return;
    }
    if (!podeGravar(destino)) {
        printf("Operacao cancelada.\n");
        return;
    }

    r = descomprimirArquivo(origem, destino, &stats);
    if (r == COMPACTA_ERRO_ORIGEM) {
        printf("Erro: nao foi possivel abrir o arquivo comprimido.\n");
        return;
    }
    if (r == COMPACTA_FORMATO_INVALIDO) {
        printf("Erro: arquivo fora do formato CFZ2 ou corrompido.\n");
        return;
    }
    if (r == COMPACTA_ERRO_DESTINO) {
        printf("Erro: nao foi possivel criar o arquivo restaurado.\n");
        return;
    }

    printf("Arquivo restaurado em: %s\n", destino);
    mostrarEstatisticas(&stats);
    printf("Integridade (CRC32): %s\n", stats.integridadeOk ? "OK" : "FALHOU - arquivo pode estar corrompido");
}

/* funcionalidade adicional: comprime com o CompactaFacil e com o gzip do sistema
   e mostra os dois resultados lado a lado, como pede o enunciado */
static void compararComGzip(void) {
    char origem[512], destinoCfz[512], destinoGz[512];
    Estatisticas stats;
    long long tamanhoOriginal, tamanhoGz;
    int r;

    printf("Caminho do arquivo para comparar: ");
    lerLinha(origem, sizeof(origem));
    if (origem[0] == '\0') {
        printf("Erro: caminho vazio.\n");
        return;
    }

    tamanhoOriginal = obterTamanhoArquivo(origem);
    if (tamanhoOriginal < 0) {
        printf("Erro: nao foi possivel abrir \"%s\".\n", origem);
        return;
    }

    snprintf(destinoCfz, sizeof(destinoCfz), "%s.cfz", origem);
    snprintf(destinoGz, sizeof(destinoGz), "%s.gz", origem);

    r = comprimirArquivo(origem, destinoCfz, &stats);
    if (r != COMPACTA_OK) {
        printf("Erro ao comprimir com o CompactaFacil.\n");
        return;
    }

    tamanhoGz = comprimirComGzip(origem, destinoGz);

    printf("\nComparacao de compressao\n");
    printf("Arquivo original : %-30s %10lld bytes\n", origem, tamanhoOriginal);
    printf("CompactaFacil    : %-30s %10" PRIu64 " bytes", destinoCfz, stats.tamanhoComprimido);
    if (tamanhoOriginal > 0)
        printf("  (razao %.4f)", (double)stats.tamanhoComprimido / (double)tamanhoOriginal);
    printf("\n");

    if (tamanhoGz == COMPACTA_CAMINHO_INSEGURO) {
        printf("gzip             : caminho contem caracteres nao suportados para comparacao.\n");
    } else if (tamanhoGz < 0) {
        printf("gzip             : indisponivel neste sistema (instale o gzip ou rode em um terminal Linux/Mac/WSL).\n");
    } else {
        printf("gzip             : %-30s %10lld bytes", destinoGz, tamanhoGz);
        if (tamanhoOriginal > 0)
            printf("  (razao %.4f)", (double)tamanhoGz / (double)tamanhoOriginal);
        printf("\n");
    }
}

int main(void) {
    char linha[32];
    int opcao;

    printf("=== CompactaFacil ===\n");

    for (;;) {
        printf("\n1 - Comprimir\n2 - Descomprimir\n3 - Comparar com gzip\n4 - Sair\nOpcao: ");
        lerLinha(linha, sizeof(linha));
        opcao = atoi(linha);

        if (opcao == 1)
            comprimir();
        else if (opcao == 2)
            descomprimir();
        else if (opcao == 3)
            compararComGzip();
        else if (opcao == 4)
            break;
        else
            printf("Opcao invalida.\n");
    }

    return 0;
}
