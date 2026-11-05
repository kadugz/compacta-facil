# CompactaFacil

Programa em C para comprimir e descomprimir arquivos de texto ou binarios usando
codificacao de Huffman. Trabalho de implementacao da disciplina MCTA028-15
(Programacao Estruturada) - UFABC, 2026.3.

> Entrega oficial feita pelo Moodle da disciplina. Este repositorio e apenas
> para controle de versao do grupo.

## Integrantes

- Ana Julia Massola Gonçalves
- Carlos Eduardo Gonçalves Santos
- Isabella Nascimento Lima 
- Pedro Henrique Kami Dell’Aringa Riccio


## Compilar

```
gcc -Wall -Wextra -std=c99 -o compactafacil main.c compactafacil.c
```

## Executar

```
./compactafacil        # Linux/Mac
compactafacil.exe      # Windows
```

Menu:

```
1 - Comprimir
2 - Descomprimir
3 - Comparar com gzip
4 - Sair
```

Detalhes de uso, formato do arquivo `.cfz` e os cenarios de teste estao em
[leia-me.txt](leia-me.txt).

## Estrutura

```
CompactaFacil/
|-- main.c            (menu e interacao com o usuario)
|-- compactafacil.c    (huffman, compressao/descompressao, CRC32, comparacao com gzip)
|-- compactafacil.h
|-- testes/            (arquivos de baixa, media e alta redundancia)
|-- leia-me.txt
```
