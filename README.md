# Signal Replication — LPC1769

Replicación de una señal analógica en tiempo real: la señal entra por el **ADC**, se almacena en dos buffers mediante **DMA** (esquema *ping-pong*) y se vuelca al **DAC** también por DMA, con una latencia de un buffer.

> Proyecto de la materia **Electrónica Digital III** — Ingeniería en Computación (FCEFyN, UNC).

**Estado:** en validación. La configuración del ADC y del DMA hacia los buffers está implementada; la salida por DAC todavía no fue verificada en hardware.

## Qué hace

1. **ADC (canal 0, P0.23)**: convierte la señal de entrada a 200 kHz de reloj del ADC. Cada conversión se dispara por el **MAT0.1 del Timer0**, que en modo *toggle* genera un flanco de subida cada 166 µs, es decir, unas 3012 muestras por segundo.
2. **GPDMA canal 0 (P2M)**: copia cada resultado de `ADGDR` a `bufferA` o `bufferB` (60 muestras cada uno). Las dos LLI se encadenan en círculo, así que los buffers se llenan alternadamente sin intervención de la CPU.
3. **GPDMA canal 1 (M2P)**: vuelca cada buffer lleno al `DACR` (P0.26), también con LLI circulares.
4. **Priming**: el DAC no arranca hasta que el ciclo de llenado completó dos buffers. El `TIMER0_IRQHandler` cuenta los matches y habilita el canal 1 cuando llega a 120.

Con 60 muestras por período, una senoidal de 50 Hz se muestrea a 3000 Hz, muy cerca de los 3012 Hz del timer.

## Parámetros

| Define | Valor | Significado |
|---|---|---|
| `NUM_SINE_SAMPLE` | 60 | Muestras por buffer |
| `ADC_FREQ` | 200000 | Reloj del ADC (Hz) |
| `MATCH_VALUE_EN_US` | 166 | Período del match del timer (µs) |
| `SINE_FREQ_IN_HZ` | 50 | Frecuencia de referencia para el timeout del DAC |

El timeout del DAC se calcula como `timeOut = PCLK_DAC / (NUM_SINE_SAMPLE × SINE_FREQ_IN_HZ)`, con `PCLK_DAC = CCLK/4`.

## Hardware

- Placa **LPC1769** (LPCXpresso).
- Entrada: **P0.23 (AD0.0)**. Rango unipolar 0–3.3 V: para una senoidal, usar offset ≈ 1.65 V y amplitud pequeña para empezar (≈ 1 Vpp).
- Salida: **P0.26 (AOUT)**.
- Masa común entre la placa, el generador de señales y el osciloscopio.
- Generador en modo **High-Z**. Con la carga de 50 Ω por defecto, la amplitud mostrada se duplica.

## Dependencias

Este proyecto **no incluye los drivers**. Usa los drivers CMSIS mejorados de la facultad:

- [LPC17xx-CMSIS-Driver-Enhancement](https://github.com/David-A-T-M/LPC17xx-CMSIS-Driver-Enhancement)

Módulos usados: `lpc17xx_adc`, `lpc17xx_dac`, `lpc17xx_gpdma`, `lpc17xx_timer`, `lpc17xx_clkpwr`.

## Compilar y correr

1. Importar en **MCUXpresso IDE** (`File → Import → Existing Projects into Workspace`).
2. Importar también el proyecto de drivers y compilarlo (genera la librería estática).
3. Verificar que este proyecto referencie la librería de drivers (Properties → C/C++ Build).
4. Build (`Debug`) y cargar a la placa.

## Estructura

```
src/signal_replicate.c         Código principal (ADC + DMA ping-pong + DAC)
src/cr_startup_lpc175x_6x.c    Startup de arranque (autogenerado)
src/crp.c                      Code Read Protection (autogenerado)
```

## Notas de implementación

- `ADC_EdgeStartConfig` tiene que llamarse **después** de `ADC_Init`, porque `ADC_Init` reescribe todo `ADCR` y borra el flanco configurado.
- Las LLI de ADC usan `srcAddr = &LPC_ADC->ADGDR`, no `ADCR`: el dato de conversión está en `ADGDR` (bits 15:4).
- El resultado de 12 bits en `ADGDR` queda alineado con el `VALUE` del `DACR` (bits 15:6) al copiarlo como HALFWORD, así que no hace falta reformatear el dato.
- En la configuración de `dmaCfgADC`, `srcMemAddr` se sobreescribe con `GPDMA_LUTPerAddr`, por lo que su valor no afecta la transferencia.
