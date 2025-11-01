# Relatório / Explicação do Trabalho

## Objetivo
Demonstrar a implementação de **múltiplas abordagens de paralelismo** sobre o *mesmo* problema de processamento de imagem, atendendo aos feedbacks:

- **Feedback 1**: “dividir em blocos” não significa necessariamente “pool de threads” — dá para fazer **atribuição estática** de blocos.
- **Feedback 4**: mostrar a ideia original de **pool de threads** (atribuição dinâmica) para cenários de custo desigual.

O problema escolhido foi: **contar quantos pixels da imagem possuem valor acima de um limiar** (`THRESHOLD=128`).

---

## Fase 0 – Leitura / Carga da Imagem
No código, para simplificar, a imagem é criada em memória com valores aleatórios:

```c
Image *img = carregar_imagem_fake(W, H);
```

Na versão final do trabalho, este ponto pode ser substituído por leitura real de arquivo (PGM, BMP, PNG, etc.).

---

## Versão A – Fatias Horizontais com Atribuição Estática

**Ideia:** dividir a imagem por linhas. Se temos `N` threads e `H` linhas, cada thread recebe `H / N` linhas (a última pode pegar o resto).

**Passos:**
1. Thread principal cria as `N` threads.
2. Cada thread recebe: `ID`, ponteiro para a imagem, `linha_ini`, `linha_fim`.
3. Cada thread percorre APENAS as suas linhas.
4. Cada thread mantém **contador_local**.
5. Ao terminar, cada thread faz:
   ```c
   pthread_mutex_lock(&g_mutex_global);
   g_contador_global += contador_local;
   pthread_mutex_unlock(&g_mutex_global);
   ```
6. A thread principal dá `pthread_join` em todas.

**Por que é estático?**  
Porque o intervalo de linhas é decidido **antes** de iniciar o processamento e **não muda**.

---

## Versão B – Blocos (Tiling) com Atribuição Estática

**Motivação (Feedback 1):** mostrar que sabemos fazer divisão por blocos *sem* usar “pool”.

**Ideia:** dividir a imagem em blocos de tamanho fixo (ex.: 64x64). Isso gera um *grid* de blocos.  
Se tivermos, por exemplo, 256 blocos e 4 threads:
- Thread 0 → blocos 0..63
- Thread 1 → blocos 64..127
- ...

**Passos:**
1. Thread principal calcula quantos blocos cabem na horizontal e vertical.
2. Cria um vetor lógico de blocos.
3. Atribui um intervalo **fixo** de blocos para cada thread.
4. Cada thread percorre **seus** blocos e conta pixels.
5. No final, faz o mesmo padrão `contador_local → mutex → contador_global`.

**Vantagem:** atende ao feedback e mostra que “usar blocos” não obriga a ter controle dinâmico.

---

## Versão C – Blocos (Tiling) com Atribuição Dinâmica (Pool)

**Motivação (Feedback 4):** mostrar a ideia de **saco de tarefas** / **work stealing simplificado**.

**Diferença chave da versão B:**  
- Na B, **cada thread já sabe** quais blocos vai processar.
- Na C, **nenhuma thread sabe** de antemão; todas disputam o **próximo bloco disponível**.

**Como funciona:**
1. A thread principal cria o mesmo grid de blocos da versão B.
2. Em vez de dividir, ela coloca um **índice global de próxima tarefa**:
   ```c
   static int g_proximo_bloco = 0;
   ```
3. Cada thread faz:
   ```c
   while (1) {
       pegar mutex de tarefas;
       pegar índice do próximo bloco;
       soltar mutex;
       se não tem mais bloco → sair;
       processar bloco;
   }
   ```
4. Ao terminar, mesma história: `contador_local` é somado ao global com o mutex global.

**Por que isso é útil?**  
Se alguns blocos forem mais pesados que outros (ex.: regiões com muito detalhe), as threads não ficam ociosas: quem terminar primeiro vai pegar outra tarefa.

**Custo extra:**  
Cada bloco exige **1 acesso ao mutex de tarefas**. Em cenários de bloco muito pequeno isso pode ficar caro.

---

## Sobre o “Contador final”

O contador final representa **quantos pixels da imagem satisfizeram o critério** (`pixel > 128`).  
Usamos isso apenas como **proxy** para “uma computação por píxel que precisa ser somada de forma segura”.

No código deste pacote o `srand(1234);` foi fixado para o resultado das três versões ficar **igual** e facilitar a comparação.

---

## Como comparar as três versões

1. **Compilar:**
   ```bash
   make
   ```

2. **Rodar A:**
   ```bash
   ./image_threads 4 1
   ```

3. **Rodar B:**
   ```bash
   ./image_threads 4 2
   ```

4. **Rodar C:**
   ```bash
   ./image_threads 4 3 64 64
   ```

Se tudo estiver certo, os três devem mostrar **o mesmo contador final** (porque a imagem agora é sempre a mesma).

---

## Conclusão

- **Versão A**: mais simples, menos overhead, boa quando carga é homogênea.
- **Versão B**: mostra entendimento de *tiling* **estático**, atende Feedback 1.
- **Versão C**: mostra entendimento de *tiling* **dinâmico**, atende Feedback 4 e permite balanceamento de carga.

Coloque estes pontos na sua entrega que você demonstra domínio do tema.
