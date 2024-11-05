    .data
prompt_nota:               .string "Informe as notas (e informe 0 para terminar o comando): "
resultado_mensagem:        .string "A media das notas e: "
erro_mensagem:             .string "Nenhuma nota foi inserida.\n"
    .text
    .globl main

main:

    li      t1, 0
    li      t2, 0               

ler_notas:

    la      a0, prompt_nota
    li      a7, 4   
    ecall


    li      a7, 5 
    ecall
    beq     a0, zero, calcula_media 

    add     t1, t1, a0          

    addi    t2, t2, 1
    j       ler_notas       

calcula_media:

    beq     t2, zero, erro       
    div     t3, t1, t2         

    la      a0, resultado_mensagem
    li      a7, 4         
    ecall

    mv      a0, t3
    li      a7, 1
    ecall

    li      a7, 10
    ecall

erro:

    la      a0, erro_mensagem
    li      a7, 4
    ecall

    li      a7, 10
    ecall
