O que ele faz (na essência)
    • O ATmega conversa com o BMP390 por I²C (no teu caso 0x76, porque SDO=GND e CSB=VCC).
    • A cada ciclo, ele:
        1. coloca o sensor em modo forced (faz uma medição);
        2. espera o data ready;
        3. lê pressão e temperatura “cruas”;
        4. aplica a calibração interna do BMP390 (aqueles coeficientes da NVM);
        5. converte pra °C e hPa;
        6. mostra no LCD 20x4.
Por que ficou melhor que o BMP180 no teu caso
    • O BMP390 é mais moderno e mais estável (menos drift e ruído).
    • A leitura ficou “na pressão local” e bateu com teu barômetro mecânico, ou seja:
        ◦ você está medindo pressão ambiente real (estação/QFE),
        ◦ e não uma pressão “corrigida” ou com offset preso (que é o tipo de coisa que faz aparecer 1016 fixo em alguns projetos antigos).
Pontos fortes do jeito que você montou
    • I²C em 1 MHz: você desacelerou o barramento (TWBR) e isso dá robustez.
    • Sem printf de float: você resolveu do jeito certo (inteiros), fica leve e confiável no AVR.
    • LCD + endereços na tela (BMP:0x76 / LCD:0x27) é ótimo pra debug.
Próximos passos naturais (se quiser evoluir)
    • Filtro/média móvel pra pressão ficar bem “lisinha”.
    • Tendência barométrica (subindo/caindo/estável) comparando com 10–30 minutos atrás.
    • Log com RTC (DS3231/DS1307): salvar data/hora + hPa + °C em EEPROM/SD.
    • Altímetro (opcional): calcular altitude aproximada a partir da pressão e uma referência.
