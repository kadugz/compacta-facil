#ifndef COMPACTAFACIL_H
#define COMPACTAFACIL_H

#include <stdint.h>

#define TAM_ALFABETO 256
#define MAX_NOS (2 * TAM_ALFABETO - 1)
#define TAM_CODIGO_MAX 256

typedef struct {
    unsigned char caractere;
    uint64_t frequencia;
    int esquerda;
    int direita;
    int ativo;
} No;

typedef struct {
    No nos[MAX_NOS];
    int totalNos;
    int raiz; /* -1 se nao houver simbolos */
} ArvoreHuffman;

typedef struct {
    char bits[TAM_CODIGO_MAX];
    int tamanho;
} Codigo;

typedef struct {
    uint64_t tamanhoOriginal;
    uint64_t tamanhoComprimido;
    double tempoSegundos;
    int modoArmazenado; /* 1 = dados gravados sem codificar (arquivo pequeno/incompressivel) */
    int integridadeOk;  /* so vale na descompressao: compara o CRC32 gravado com o recalculado */
} Estatisticas;

#define COMPACTA_OK 0
#define COMPACTA_ERRO_ORIGEM -1
#define COMPACTA_ERRO_DESTINO -2
#define COMPACTA_FORMATO_INVALIDO -3
#define COMPACTA_CAMINHO_INSEGURO -4

/* huffman: frequencias, arvore e codigos */
void contarFrequencias(const unsigned char *dados, uint64_t tamanho,
                        uint64_t frequencias[TAM_ALFABETO]);
void construirArvore(ArvoreHuffman *arvore, const uint64_t frequencias[TAM_ALFABETO]);
void gerarCodigos(const ArvoreHuffman *arvore, Codigo codigos[TAM_ALFABETO]);

/* checksum simples para validar integridade apos a descompressao */
uint32_t calcularCRC32(const unsigned char *dados, uint64_t tamanho);

/* compressao: leitura/escrita do arquivo .cfz */
long long obterTamanhoArquivo(const char *caminho);
int comprimirArquivo(const char *origem, const char *destino, Estatisticas *stats);
int descomprimirArquivo(const char *origem, const char *destino, Estatisticas *stats);

/* funcionalidade adicional: compara o resultado do CompactaFacil com o gzip do sistema */
long long comprimirComGzip(const char *origem, const char *destinoGz);

#endif
