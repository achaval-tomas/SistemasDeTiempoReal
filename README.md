Tomás Achával Berzero
Informe final de proyecto, Sistemas de Tiempo Real - 2026

# Variómetro para vuelo en parapente: Detector de Velocidad Vertical en Tiempo Real

Un variómetro para vuelo en parapente es un sistema que provee feedback auditivo y visual en tiempo real informando al piloto sobre su velocidad vertical, ya sea de ascenso o descenso. Está basado en la medición de cambios en la presión atmosférica , los cuales son inversamente proporcionales a los cambios en la altitud del piloto. El propósito de este sistema es facilitar la detección de corrientes de aire ascendente para el piloto, las cuales permiten ganar altura para luego planear largas distancias. El sistema contendrá un menú principal donde se podrán ajustar configuraciones o iniciar/frenar el “modo variómetro”. Para esto se utilizará un buzzer pasivo para emitir los sonidos necesarios, una pantalla LCD, un sensor de presión atmosférica y un encoder rotativo con pulsador para navegar el menú.


## Objetivos del Proyecto

El objetivo principal del proyecto es desarrollar un sistema de variómetro para parapente con feedback visual y auditivo en tiempo real, configurable según las preferencias del piloto.

### Objetivos específicos

- Implementar un filtro de kalman para la medición de presión y tasa de ascenso con parámetros ajustables.
- Garantizar un tiempo de reacción y de emisión de sonido acorde a los parámetros seleccionados por el usuario (hasta <200ms en el modo más sensible).
- Desarrollar un sistema de feedback auditivo capaz de variar dinámicamente la frecuencia y cadencia del sonido en función de la tasa de ascenso/descenso.
- Construir una interfaz de usuario interactiva para la visualización de datos de vuelo y un menú principal para la configuración de parámetros del sistema (tonos, umbrales, sensibilidad).

## Descripción Funcional

### Diagrama de Bloques

![](/z_imgs/architecture.jpg)

#### Entradas:

- Sensor de Presión Atmosférica y Temperatura (BMP280): Fuente de datos
principal para el cálculo de altitud y velocidad vertical. Se lee a través de un bus de
comunicación I2C.

- Rotación del Encoder: Utilizada por el usuario para la navegación del menú y configuración de parámetros. Se lee mediante un Timer de hardware configurado
en modo Encoder.
- Pulsador del Encoder: Utilizado por el usuario para la selección de opciones y cambio de modos (Ej. salir del modo vuelo). Se detecta mediante una entrada GPIO configurada con interrupciones (EXTI).

#### Salidas:

- Feedback Auditivo (Buzzer Pasivo): Actuador principal del sistema que emite los tonos de ascenso/descenso en tiempo real. Es manejado mediante una salida de PWM generada por un Timer de hardware para controlar su frecuencia.
- Interfaz de Usuario (Display LCD): Interfaz para mostrar el menú de configuraciones y los datos de vuelo en tiempo real. Se actualiza mediante comandos enviados a través de un bus I2C.

## Arquitectura y diseño en FreeRTOS

La única modificación de esta sección se encuentra en la parte de interrupciones, donde se agregó un software timer en modo one-shot que interactúa con las interrupciones del pulsador para detectar pulsaciones largas. El resto se mantiene igual, debido a que al momento de realizar el primer informe el proyecto ya estaba en una etapa avanzada.

### Tareas Principales y Prioridades

- Tarea de Interfaz de Usuario:
    - Prioridad baja, se requiere tiempo de respuesta “humano”.
    - Responde a eventos de un encoder rotativo, permitiendo la navegación de un menú, cambios de configuraciones e inicio/frenado de vuelos.

- Tarea de Sensado y Filtrado:
    - Prioridad máxima para garantizar frecuencia de muestreo constante.
    - Lee los datos del sensor de presión atmosférica y aplica un filtro de Kalman a una frecuencia constante.

- Tarea de Control de Sonido:
    - Segunda prioridad más alta. Luego de que se produzca un dato por la tarea de sensado, lo primero que debe ocurrir es el feedback auditivo. No es prioridad máxima ya que si la tarea de sensado está leyendo un nuevo dato, conviene esperar antes de emitir un sonido que quizás ya no es relevante.
    - Esta tarea emite los sonidos a distintas frecuencias y cadencias que indican ascenso/descenso al piloto a partir de los datos obtenidos del sensor. También emite un sonido cuando el usuario modifica la configuración de volumen.

- Tarea de Control de Display:
    - Prioridad mínima, tiempo de respuesta “humano”.
    - Esta tarea recibe eventos de actualización de menú o datos de vuelo y muestra lo requerido en el panel LCD.

- Extra: Software Timer para detectar rotaciones del encoder
    - Modo AutoReload con período de 50ms
    - Asociado a una función de callback que lee el valor del timer asociado al encoder e informa al menú ante eventos de rotación.
    - Se puede deshabilitar en modo de vuelo para reducir jitter ya que las rotaciones sólo interesan en el menú principal y rehabilitar tras salir del modo de vuelo mediante una pulsación larga.

### Sincronización y Comunicación

Una vez iniciado el “modo de vuelo”, las tareas de control de sonido y de control de display dependen exclusivamente de los datos generados por la tarea de sensado y filtrado (se bloquean esperando estos datos en una **queue** ), y esta última se encargará de enviarles siempre el dato más reciente mediante el servicio **xQueueOverwrite**.

Se utilizará una **queue de eventos** a través de la cual se bloquea la tarea de interfaz de usuario hasta recibir eventos de rotación, pulsación larga o pulsación corta del encoder para luego procesarlos según corresponda.

La tarea de control de display recibirá distintos comandos mediante una **queue**. Cada comando puede tener datos asociados o no (prender, apagar, mostrar menú, mostrar datos de vuelo, etc) y estos comandos pueden ser enviados desde la tarea de interfaz de usuario o la tarea de sensado, dependiendo el estado del sistema (en vuelo o en menú).

Se utilizarán **Direct to Task Notifications** (o un semáforo binario) para el inicio/frenado de la tarea de sensado.

### Interrupciones

La principal fuente de interrupciones es el pulsador del encoder, en donde se generan interrupciones de GPIO ante Falling (presionado) y Rising (liberado). A esto se le suma un **one-shot timer** que se lanza cuando el encoder es presionado y su callback es ejecutada sólo si pasa suficiente tiempo para ser considerado un “long press” antes de que sea liberado. La coordinación entre este timer y la interrupción que se genera al soltar el pulsador es manejada por un semáforo no bloqueante el cual asegura que sólo se genera un evento por pulsación (long press o click respectivamente). Esta forma de manejar las pulsaciones resultó ser mucho más intuitiva desde el punto de vista del usuario que la idea anterior de esperar a que se suelte para calcular la duración y luego decidir qué evento generar.

## Decisiones de diseño

**Interfaz gráfica durante un vuelo**

![](/z_imgs/flight_ui.png)

El tiempo de vuelo, temperatura, altitudes y gráfico de historial se actualizan una vez por segundo. El valor de vario “V” y la flecha de indicación de ascenso/descenso se actualizan cada 200ms ya que son los más importantes para el piloto. Todas las actualizaciones reflejan el valor más reciente generado por el sensor.

Un pequeño detalle es que la temperatura que se muestra es la del sensor de presión atmosférica y temperatura que está dentro del dispositivo, por lo que usualmente es mayor a la temperatura en el exterior. Considero que de todos modos es útil para detectar un posible sobrecalentamiento.

La decisión de que el gráfico de historial de vuelo se actualice solo una vez por segundo es para dar una mayor cantidad de información al piloto. De esta forma resulta bastante útil debido a que ante la entrada a una corriente de aire ascendente, los pilotos esperan 3 o 4 segundos para comenzar a girar e intentar mantenerse en ella. Esto se vería claramente reflejado en el gráfico, con 4 símbolos de ascenso seguidos.

**Prioridades de las tareas y software timers**
La tarea de sensado y filtrado tiene la prioridad máxima entre las tareas ya que se debe asegurar una frecuencia de muestreo constante para el funcionamiento ideal del filtro.

La tarea de control de sonido es la segunda de mayor prioridad ya que cada vez que se genera un dato de cambio de altitud, el primer feedback que se debe generar es el auditivo.

La tarea de control de display es la de menor prioridad ya que el feedback visual es de menor importancia que el auditivo. La idea es que siempre que uno mire el display, el dato reflejado sea el más reciente, por lo que es preferible que sea bloqueada cuando ya sea se está procesando un nuevo dato del sensor o una acción del usuario. Esta decisión tiene el costo de que el display se actualiza levemente más lento que en caso de una mayor prioridad, pero asegura que siempre que se termina de actualizar, lo hace con el dato más reciente.

La tarea de Interfaz de usuario tiene la tercer mayor prioridad. Cuando el usuario navega el menú, esta interactúa principalmente con la tarea del display, y tiene mayor prioridad que ella por las razones mencionadas anteriormente. Si el usuario genera múltiples eventos a través del encoder (rotar, click, mantener pulsado), el display se actualiza directamente al estado final, sin terminar de mostrar los estados intermedios.

Los software timers relacionados al encoder tienen la prioridad máxima (mayor a la de cualquier tarea) para que sus callbacks (que son muy sencillas y no bloqueantes) sean interpretadas como “interrupciones”.

A pesar de que las tareas tienen prioridades en orden descendiente, el sistema no funciona a modo de superloop:

- La tarea de sonido permanece bloqueada por la duración completa de cada sonido emitido. Siempre que se desbloquea durante un vuelo, tiene disponible en su queue el dato más reciente de cambio de altitud para emitir (o no) el próximo sonido. Esta decisión (en lugar de modificar un tono mientras suena) fue tomada para coincidir con el funcionamiento de los sistemas profesionales.
- La tarea de display funciona de dos formas. En el menú principal, simplemente se actualiza ante cada evento generado por el encoder. Durante un vuelo, se separa en dos partes. Los datos de mayor importancia en cuanto a “tiempo real” se actualizan cada 200ms para mantener legibilidad, y los de menor importancia cada 1 segundo. El resto del tiempo permanece bloqueada.


Como ambas se bloquean en distintos momentos, el orden en el que se ejecutan es aleatorio. Si ambas están disponibles a la vez, la que se ejecutará primero es la que emite el feedback auditivo por su mayor prioridad.

**Configuración del sistema**
Los parámetros de configuración se guardan en una estructura compartida por todas las tareas. Se pueden cambiar desde el menú principal y no durante un vuelo. Esta decisión fue por lo que se acostumbra en el deporte, un piloto no se pone a reconfigurar sus dispositivos durante un vuelo. El beneficio de esta decisión es que no es necesario proteger las escrituras con algún mecanismo de exclusión mutua ya que durante el vuelo sólo se leen sus valores, y el orden de prioridades de las tareas que actúan en el menú asegura que las escrituras a las mismas no tienen condiciones de carrera.

**Manejo de las acciones del usuario con el encoder**
Aquí debí tomar múltiples decisiones que fueron cambiando con el tiempo. La versión final tiene lo siguiente:

- Software timer periódico que realiza polling al timer de hardware asociado a las rotaciones del encoder y genera eventos en caso de que haya alguna rotación.
- Software timer one-shot que se dispara ante la interrupción generada cuando el usuario pulsa el botón, y su callback es ejecutada si pasa suficiente tiempo sin soltarlo para ser considerado un long press. En caso de ser liberado antes, se genera un evento de “click” por la interrupción de liberación.

Esto requirió un mecanismo de exclusión mutua para solucionar una condición de carrera muy poco probable pero posible: Si se comenzaba a ejecutar la callback del one-shot timer y antes de generar el evento de long-press se generaba la interrupción del pulsador liberado, entonces era posible que se generen ambos eventos a la vez.

Como no alcanzaba con una variable de control, utilicé un semáforo a modo de “token” de un sólo uso. Cuando comienza una pulsación, un semáforo binario pone el “token” a disposición y dispara el one-shot timer. El primero que reclame el token, ya sea el one-shot timer o la interrupción por liberación, será quien genera su evento (long press o click respectivamente), y el otro no hará nada. Más aún, si se libera mucho antes de tiempo, el one-shot timer es frenado.

Otra decisión fue hacer que el timer periódico que lee rotaciones tenga la opción de ser deshabilitado. Esto se usa en dos momentos para evitar la generación de eventos innecesarios y reducir el trabajo de la “daemon” task:
- Cuando el sistema está apagado y sólo espera una pulsación larga para encenderse.
- Cuando el sistema está en modo de vuelo y sólo espera una pulsación larga para volver al menú.

Esta decisión es la que permite dar prioridad máxima a los software timers para que funcionen como interrupciones, sin introducir “jitter” durante un vuelo.

**Manejo de errores de HAL**
Los errores de HAL son muy poco frecuentes y durante el desarrollo sólo ocurrieron cuando las conexiones físicas eran poco confiables. Por ello tomé la decisión de que ante un error de HAL, se muestre en el display un mensaje al usuario informando del mismo (en caso de que el error no haya sido en el display) y luego de 5 segundos se resetee el sistema completo.

## Métodos de prueba y validación

A continuación describiré las pruebas que realicé sobre el sistema completo.

Lo utilicé de manera continua por más de una hora y no mostró fallas. El uso fue intensivo, intentando generar errores con casos extraños o comportamientos inesperados. También probé el modo de vuelo durante una hora entera, y el dispositivo no mostró sobrecalentamiento ni descalibración. Debajo una foto del momento donde se superó la hora de vuelo.

![](/z_imgs/flight_time.jpg)

Lo comparé con el instrumento de vuelo Flymaster GPS LS2 y mostraron comportamiento similar en cuanto a tiempo de reacción y ruido de medición.

También lo puse a prueba bajo un usuario independiente que desconoce los detalles del proyecto. Este fue mi padre, piloto de parapente con más de 4 años de experiencia. En cuanto al modo de uso, le pareció fácil de entender y logró interactuar con el menú sin dificultades. Aunque no lo utilizó durante un vuelo, expresó que el tiempo de reacción logrado y el bajo nivel de ruido de medición se sintieron comparables o incluso mejor que algunos de sus instrumentos (app de celular con barómetro, flymaster, otros variómetros). Esto respalda la decisión de utilizar el filtro de Kalman. Una observación que realizó fue que el encoder rotativo quizás no es ideal para un instrumento de vuelo debido a que los pilotos utilizan guantes y quizás no logran percibir los “clicks” de cada paso de rotación.

## Algunos problemas encontrados

El proyecto llevó muchas horas de desarrollo, y junto a ellas, de debugging. Por ello es imposible listar todos los problemas que se encontraron y solucionaron, pero los mencionados a continuación fueron algunos de los más importantes.

➔ Sensor BMP280 no detectado / datos erróneos. La causa era simplemente una conexión inestable de hardware. La solución fue soldar el componente.

➔ Race condition en manejo de pulsación larga. La solución fue explicada anteriormente con el token de un solo uso.

➔ Notificaciones de inicio/frenado a la tarea del sensor desincronizables. La causa era la siguiente. Durante la inicialización del sensor, este toma un promedio de 3 segundos de datos de presión atmosférica para partir de un valor estable. En ese lapso de tiempo, era posible enviar múltiples notificaciones a la tarea, por ejemplo “apagar” y luego “prender”. Cuando terminaba de inicializarse, efectivamente detectaba la notificación, pero reseteaba el contador de la misma, es decir, sólo se “apagaba” mientras que la UI esperaba que se haya apagado y encendido generando una desincronización. La solución fue directa: no limpiar el contador de notificaciones en cada lectura.

➔ Uno de los problemas que más tiempo llevó de detectar fue que se generaban eventos de “long press” no deseados, causando ya sea un fin de vuelo o que el sistema se apague de manera inesperada. La causa era muy sutil y estaba relacionada al hardware. Era posible que al soltar el botón se genere (por ruido) otra interrupción de “falling” como si se hubiera presionado nuevamente, causando que se dispare el one-shot timer y genere el evento no deseado. La implementación inicial sólo tenía un “debounce” de liberación. La solución fue hacer que el debounce sea bidireccional y agregar una verificación cuando expira el one-shot timer que asegura que el botón aún esté siendo presionado.

## Conclusiones

![](/z_imgs/full_device.jpg)

Logré implementar un sistema de variómetro robusto, confiable y completamente configurable. Para ello se utilizaron múltiples herramientas de FreeRTOS como tareas, timers periódicos y de un solo uso, queues, semáforos y notificaciones. El sistema interactúa con periféricos (LCD, bmp280, encoder, buzzer) cuyas librerías utilizan funciones que provee la HAL del microcontrolador.

La mayor vulnerabilidad actual del sistema es la fiabilidad del hardware debido a que está armado dentro de una caja a modo de prototipo y un movimiento muy brusco podría desconectar algún componente.

Algunas de las principales fuentes de información para llevar a cabo el desarrollo del proyecto fueron la Documentación de FreeRTOS, la hoja de especificaciones del microcontrolador, la datasheet del sensor bmp280 y el debugger integrado en la extensión de VSCode para STM32.


## Trabajos futuros

Hay varios caminos posibles para continuar el desarrollo del sistema, a continuación menciono algunos que quizás llevaré a cabo.

★ Log de datos de vuelo a una microSD, permitiendo recrear el gráfico de altitud sobre tiempo. Esto sería implementado en una tarea de mínima prioridad.

★ Menú de récords y datos del usuario. Permitiría ver el tiempo de vuelo acumulado, la máxima tasa de ascenso, el vuelo de mayor duración, etc.

★ Detección automática de inicio de vuelo. Esto es una función de algunos variómetros profesionales y requiere un acelerómetro o GPS. Cuando se detecta una velocidad _horizontal_ de 10-12km/h, se inicia automáticamente el vuelo.

★ Más modos de vuelo. Algunos pilotos prefieren ver más datos, otros menos. Una opción sería que haya 3 pantallas de vuelo seleccionables con clicks y que cada una de ellas muestre más o menos datos.

★ Manejo de errores específicos. Esto requeriría que cada tipo de error tenga su propio manejador donde algunos frenarían el sistema, otros reintentarían la acción y otros serían ignorados.

★ Reducir el consumo de energía lo máximo que sea posible manteniendo toda la funcionalidad.

