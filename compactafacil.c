#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "compactafacil.h"

/* tamanho do cabecalho comum a qualquer arquivo .cfz: magia(4) + modo(1) + tamanho(8) + crc(4) */
#define TAM_CABECALHO_BASE 17
/* cada entrada da tabela de frequencias: simbolo(1) + frequencia(4) */
#define TAM_ENTRADA_TABELA 5

/* ===== Huffman: frequencias, arvore, codigos ===== */

void contarFrequencias(const unsigned char *dados, uint64_t tamanho,
                        uint64_t frequencias[TAM_ALFABETO]) {
    uint64_t i;
    memset(frequencias, 0, TAM_ALFABETO * sizeof(uint64_t));
    for (i = 0; i < tamanho; i++)
        frequencias[dados[i]]++;
}

/* indice do no ativo de menor frequencia, ignorando "exceto" */
static int menorAtivo(const ArvoreHuffman *arvore, int exceto) {
    int i, menor = -1;
    for (i = 0; i < arvore->totalNos; i++) {
        if (i == exceto || !arvore->nos[i].ativo)
            continue;
        if (menor == -1 || arvore->nos[i].frequencia < arvore->nos[menor].frequencia)
            menor = i;
    }
    return menor;
}

void construirArvore(ArvoreHuffman *arvore, const uint64_t frequencias[TAM_ALFABETO]) {
    int i;

    arvore->totalNos = 0;
    arvore->raiz = -1;

    for (i = 0; i < TAM_ALFABETO; i++) {
        No *no;
        if (frequencias[i] == 0)
            continue;
        no = &arvore->nos[arvore->totalNos];
        no->caractere = (unsigned char)i;
        no->frequencia = frequencias[i];
        no->esquerda = -1;
        no->direita = -1;
        no->ativo = 1;
        arvore->totalNos++;
    }

    if (arvore->totalNos == 0)
        return; /* arquivo vazio */

    if (arvore->totalNos == 1) {
        arvore->raiz = 0;
        return;
    }

    for (;;) {
        int a = menorAtivo(arvore, -1);
        int b = menorAtivo(arvore, a);
        No *pai;

        if (b == -1) {
            arvore->raiz = a;
            break;
        }

        pai = &arvore->nos[arvore->totalNos];
        pai->caractere = 0;
        pai->frequencia = arvore->nos[a].frequencia + arvore->nos[b].frequencia;
        pai->esquerda = a;
        pai->direita = b;
        pai->ativo = 1;

        arvore->nos[a].ativo = 0;
        arvore->nos[b].ativo = 0;
        arvore->totalNos++;
    }
}

static void percorrer(const ArvoreHuffman *arvore, int indice, Codigo codigos[TAM_ALFABETO],
                       char *caminho, int profundidade) {
    const No *no = &arvore->nos[indice];

    if (no->esquerda == -1 && no->direita == -1) {
        Codigo *c = &codigos[no->caractere];
        c->tamanho = profundidade; /* 0 = unico simbolo do arquivo */
        memcpy(c->bits, caminho, profundidade);
        return;
    }

    caminho[profundidade] = '0';
    percorrer(arvore, no->esquerda, codigos, caminho, profundidade + 1);

    caminho[profundidade] = '1';
    percorrer(arvore, no->direita, codigos, caminho, profundidade + 1);
}

void gerarCodigos(const ArvoreHuffman *arvore, Codigo codigos[TAM_ALFABETO]) {
    char caminho[TAM_CODIGO_MAX];
    int i;

    for (i = 0; i < TAM_ALFABETO; i++)
        codigos[i].tamanho = -1; /* simbolo nao usado no arquivo */

    if (arvore->raiz == -1)
        return;

    percorrer(arvore, arvore->raiz, codigos, caminho, 0);
}

/* ===== Integridade: CRC32 (mesmo algoritmo usado pelo gzip) ===== */

uint32_t calcularCRC32(const unsigned char *dados, uint64_t tamanho) {
    uint32_t crc = 0xFFFFFFFFu;
    uint64_t i;
    int b;

    for (i = 0; i < tamanho; i++) {
        crc ^= dados[i];
        for (b = 0; b < 8; b++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ===== Compressao: leitura/escrita do arquivo .cfz ===== */

long long obterTamanhoArquivo(const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    long long tamanho;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    tamanho = ftell(f);
    fclose(f);
    return tamanho;
}

/* empacota bits em bytes, MSB primeiro */
typedef struct {
    FILE *arquivo;
    unsigned char buffer;
    int bits;
} EscritorBits;

static void escritorIniciar(EscritorBits *e, FILE *arquivo) {
    e->arquivo = arquivo;
    e->buffer = 0;
    e->bits = 0;
}

static void escritorBit(EscritorBits *e, int bit) {
    e->buffer = (unsigned char)((e->buffer << 1) | (bit & 1));
    e->bits++;
    if (e->bits == 8) {
        fputc(e->buffer, e->arquivo);
        e->buffer = 0;
        e->bits = 0;
    }
}

static void escritorFinalizar(EscritorBits *e) {
    if (e->bits > 0) {
        e->buffer = (unsigned char)(e->buffer << (8 - e->bits));
        fputc(e->buffer, e->arquivo);
        e->buffer = 0;
        e->bits = 0;
    }
}

typedef struct {
    FILE *arquivo;
    unsigned char buffer;
    int bits; /* bits ainda nao lidos no buffer */
} LeitorBits;

static void leitorIniciar(LeitorBits *l, FILE *arquivo) {
    l->arquivo = arquivo;
    l->buffer = 0;
    l->bits = 0;
}

static int leitorBit(LeitorBits *l) {
    int c;
    if (l->bits == 0) {
        c = fgetc(l->arquivo);
        if (c == EOF)
            return -1;
        l->buffer = (unsigned char)c;
        l->bits = 8;
    }
    l->bits--;
    return (l->buffer >> l->bits) & 1;
}

int comprimirArquivo(const char *origem, const char *destino, Estatisticas *stats) {
    FILE *fOrigem, *fDestino;
    unsigned char *dados;
    long long tamanho;
    uint64_t tamanhoOriginal, i;
    uint64_t frequencias[TAM_ALFABETO];
    ArvoreHuffman arvore;
    Codigo codigos[TAM_ALFABETO];
    EscritorBits escritor;
    clock_t inicio, fim;
    uint32_t crc;
    uint16_t numSimbolos;
    uint64_t bitsTotal;
    uint64_t tamanhoHuffman, tamanhoArmazenado;
    int modoArmazenado;

    inicio = clock();

    tamanho = obterTamanhoArquivo(origem);
    if (tamanho < 0)
        return COMPACTA_ERRO_ORIGEM;
    tamanhoOriginal = (uint64_t)tamanho;

    fOrigem = fopen(origem, "rb");
    if (!fOrigem)
        return COMPACTA_ERRO_ORIGEM;

    dados = NULL;
    if (tamanhoOriginal > 0) {
        dados = (unsigned char *)malloc(tamanhoOriginal);
        if (!dados || fread(dados, 1, tamanhoOriginal, fOrigem) != tamanhoOriginal) {
            free(dados);
            fclose(fOrigem);
            return COMPACTA_ERRO_ORIGEM;
        }
    }
    fclose(fOrigem);

    crc = calcularCRC32(dados, tamanhoOriginal);

    contarFrequencias(dados, tamanhoOriginal, frequencias);
    construirArvore(&arvore, frequencias);
    gerarCodigos(&arvore, codigos);

    /* decide entre gravar codificado (huffman) ou cru (armazenado), o que for menor.
       isso evita que arquivos pequenos ou de simbolos muito variados saiam maiores
       do que o original: no pior caso o arquivo .cfz so cresce TAM_CABECALHO_BASE bytes. */
    numSimbolos = 0;
    bitsTotal = 0;
    for (i = 0; i < TAM_ALFABETO; i++)
        if (frequencias[i] > 0)
            numSimbolos++;
    for (i = 0; i < tamanhoOriginal; i++)
        bitsTotal += (uint64_t)codigos[dados[i]].tamanho;

    tamanhoHuffman = TAM_CABECALHO_BASE + 2 + (uint64_t)numSimbolos * TAM_ENTRADA_TABELA
                     + (bitsTotal + 7) / 8;
    tamanhoArmazenado = TAM_CABECALHO_BASE + tamanhoOriginal;
    modoArmazenado = (tamanhoArmazenado <= tamanhoHuffman);

    fDestino = fopen(destino, "wb");
    if (!fDestino) {
        free(dados);
        return COMPACTA_ERRO_DESTINO;
    }

    fwrite("CFZ2", 1, 4, fDestino);
    fputc(modoArmazenado ? 1 : 0, fDestino);
    fwrite(&tamanhoOriginal, sizeof(uint64_t), 1, fDestino);
    fwrite(&crc, sizeof(uint32_t), 1, fDestino);

    if (modoArmazenado) {
        if (tamanhoOriginal > 0)
            fwrite(dados, 1, tamanhoOriginal, fDestino);
    } else {
        fwrite(&numSimbolos, sizeof(uint16_t), 1, fDestino);
        for (i = 0; i < TAM_ALFABETO; i++) {
            unsigned char simbolo;
            uint32_t freq;
            if (frequencias[i] == 0)
                continue;
            simbolo = (unsigned char)i;
            freq = (uint32_t)frequencias[i];
            fwrite(&simbolo, 1, 1, fDestino);
            fwrite(&freq, sizeof(uint32_t), 1, fDestino);
        }

        escritorIniciar(&escritor, fDestino);
        for (i = 0; i < tamanhoOriginal; i++) {
            Codigo *c = &codigos[dados[i]];
            int b;
            for (b = 0; b < c->tamanho; b++)
                escritorBit(&escritor, c->bits[b] - '0');
        }
        escritorFinalizar(&escritor);
    }

    fim = clock();

    if (stats) {
        stats->tamanhoOriginal = tamanhoOriginal;
        stats->tamanhoComprimido = (uint64_t)ftell(fDestino);
        stats->tempoSegundos = (double)(fim - inicio) / CLOCKS_PER_SEC;
        stats->modoArmazenado = modoArmazenado;
        stats->integridadeOk = 1;
    }

    fclose(fDestino);
    free(dados);
    return COMPACTA_OK;
}

int descomprimirArquivo(const char *origem, const char *destino, Estatisticas *stats) {
    FILE *fOrigem, *fDestino;
    char magia[4];
    unsigned char modoByte;
    uint64_t tamanhoOriginal, i;
    uint32_t crcEsperado, crcCalculado;
    uint64_t frequencias[TAM_ALFABETO];
    ArvoreHuffman arvore;
    LeitorBits leitor;
    clock_t inicio, fim;
    uint16_t numSimbolos;
    unsigned char *restaurado;

    inicio = clock();

    fOrigem = fopen(origem, "rb");
    if (!fOrigem)
        return COMPACTA_ERRO_ORIGEM;

    if (fread(magia, 1, 4, fOrigem) != 4 || memcmp(magia, "CFZ2", 4) != 0) {
        fclose(fOrigem);
        return COMPACTA_FORMATO_INVALIDO;
    }
    if (fread(&modoByte, 1, 1, fOrigem) != 1 ||
        fread(&tamanhoOriginal, sizeof(uint64_t), 1, fOrigem) != 1 ||
        fread(&crcEsperado, sizeof(uint32_t), 1, fOrigem) != 1) {
        fclose(fOrigem);
        return COMPACTA_FORMATO_INVALIDO;
    }

    restaurado = NULL;
    if (tamanhoOriginal > 0) {
        restaurado = (unsigned char *)malloc(tamanhoOriginal);
        if (!restaurado) {
            fclose(fOrigem);
            return COMPACTA_ERRO_DESTINO;
        }
    }

    if (modoByte) {
        if (tamanhoOriginal > 0 && fread(restaurado, 1, tamanhoOriginal, fOrigem) != tamanhoOriginal) {
            free(restaurado);
            fclose(fOrigem);
            return COMPACTA_FORMATO_INVALIDO;
        }
    } else {
        if (fread(&numSimbolos, sizeof(uint16_t), 1, fOrigem) != 1) {
            free(restaurado);
            fclose(fOrigem);
            return COMPACTA_FORMATO_INVALIDO;
        }

        memset(frequencias, 0, sizeof(frequencias));
        for (i = 0; i < numSimbolos; i++) {
            unsigned char simbolo;
            uint32_t freq;
            if (fread(&simbolo, 1, 1, fOrigem) != 1 || fread(&freq, sizeof(uint32_t), 1, fOrigem) != 1) {
                free(restaurado);
                fclose(fOrigem);
                return COMPACTA_FORMATO_INVALIDO;
            }
            frequencias[simbolo] = freq;
        }

        construirArvore(&arvore, frequencias);
        leitorIniciar(&leitor, fOrigem);

        for (i = 0; i < tamanhoOriginal; i++) {
            int indice = arvore.raiz;
            while (arvore.nos[indice].esquerda != -1 || arvore.nos[indice].direita != -1) {
                int bit = leitorBit(&leitor);
                if (bit < 0) {
                    free(restaurado);
                    fclose(fOrigem);
                    return COMPACTA_FORMATO_INVALIDO;
                }
                indice = bit ? arvore.nos[indice].direita : arvore.nos[indice].esquerda;
            }
            restaurado[i] = arvore.nos[indice].caractere;
        }
    }

    crcCalculado = calcularCRC32(restaurado, tamanhoOriginal);

    fDestino = fopen(destino, "wb");
    if (!fDestino) {
        free(restaurado);
        fclose(fOrigem);
        return COMPACTA_ERRO_DESTINO;
    }
    if (tamanhoOriginal > 0)
        fwrite(restaurado, 1, tamanhoOriginal, fDestino);

    fim = clock();

    if (stats) {
        stats->tamanhoOriginal = tamanhoOriginal;
        stats->tamanhoComprimido = (uint64_t)obterTamanhoArquivo(origem);
        stats->tempoSegundos = (double)(fim - inicio) / CLOCKS_PER_SEC;
        stats->modoArmazenado = modoByte ? 1 : 0;
        stats->integridadeOk = (crcCalculado == crcEsperado);
    }

    free(restaurado);
    fclose(fOrigem);
    fclose(fDestino);
    return COMPACTA_OK;
}

/* ===== Funcionalidade adicional: comparacao com o gzip do sistema ===== */

/* bloqueia caracteres que escapariam das aspas no comando de shell montado abaixo */
static int caminhoSeguroParaComando(const char *caminho) {
    const char *proibidos = "\"`$;|&<>\n\r";
    size_t i;
    for (i = 0; caminho[i] != '\0'; i++)
        if (strchr(proibidos, caminho[i]) != NULL)
            return 0;
    return 1;
}

long long comprimirComGzip(const char *origem, const char *destinoGz) {
    char comando[2048];
    int r;

    if (!caminhoSeguroParaComando(origem) || !caminhoSeguroParaComando(destinoGz))
        return COMPACTA_CAMINHO_INSEGURO;

#ifdef _WIN32
    snprintf(comando, sizeof(comando), "gzip -c \"%s\" > \"%s\" 2>NUL", origem, destinoGz);
#else
    snprintf(comando, sizeof(comando), "gzip -c \"%s\" > \"%s\" 2>/dev/null", origem, destinoGz);
#endif

    r = system(comando);
    if (r != 0)
        return -1; /* gzip indisponivel neste sistema */

    return obterTamanhoArquivo(destinoGz);
}
