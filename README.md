# Trabalho Concorrente – Versões A, B e C

Este repositório demonstra **três** estratégias de paralelismo para processamento de imagem em C usando **pthreads**, seguindo exatamente o enunciado:

1. **Versão A – Fatias Horizontais com Atribuição Estática**  
2. **Versão B – Blocos (Tiling) com Atribuição Estática** *(Feedback 1)*
3. **Versão C – Blocos (Tiling) com Atribuição Dinâmica (Pool de Threads)** *(Feedback 4)*

## Estrutura do repo

```text
.
├── src/
│   └── image_threads.c   # código completo com as 3 versões
├── Makefile
└── docs/
    └── RELATORIO.md      # explicação detalhada p/ entregar
```

## Como compilar

```bash
make
# ou
gcc src/image_threads.c -o image_threads -lpthread -O3 -Wall -Wextra
```

## Como rodar

Formato geral:

```bash
./image_threads [threads] [versao] [bloco_w] [bloco_h] [width] [height]
```

- `threads`: número de threads (padrão 4)
- `versao`:
  - `1` → Versão A (linhas)
  - `2` → Versão B (blocos estáticos)
  - `3` → Versão C (blocos dinâmicos / pool)
- `bloco_w`, `bloco_h`: usados **apenas** nas versões 2 e 3 (padrão 64x64)
- `width`, `height`: tamanho da imagem fake (padrão 1024x768)

### Exemplos

```bash
# Versão A – fatias horizontais
./image_threads 4 1

# Versão B – blocos estáticos (64x64)
./image_threads 4 2

# Versão C – blocos dinâmicos (64x64)
./image_threads 4 3 64 64

# Imagem 1920x1080, versão C, 8 threads, blocos 128x128
./image_threads 8 3 128 128 1920 1080
```

## O que o "Contador final" significa?

O programa percorre **todos os pixels** da imagem e conta **quantos pixels têm valor maior que 128** (constante `THRESHOLD`).  
Isso é apenas uma **tarefa de exemplo** para mostrar o padrão:

> **contador_local → mutex → contador_global**

Em um trabalho real, você trocaria isso por “contar pixels de uma cor”, “aplicar máscara”, etc.

## Por que às vezes os valores mudam?

No código a imagem é gerada com `rand()`.  
Neste pacote eu já deixei:

```c
srand(1234);
```

→ Com isso, **todas as versões contam a mesma coisa**, então dá pra comparar A × B × C numa boa.

---

Gerado em: 2025-11-01T21:20:27
