## Proyecto de Sistemas de Tiempo Real 2026, FaMAF UNC.

### Variómetro + FreeRTOS

#### Componenetes utilizados
- Nucleo H533RE
- BMP280 (sensor barométrico y de temperatura)
- Buzzer Pasivo
- 16x2 LCD Display
- Encoder rotativo (aún no implementado)

#### Estructura de tareas
- **VariometerTask:**
Tarea encargada de leer el sensor barométrico. Es de prioridad media y se encarga de cargar los datos del sensor, preprocesados, en las QUEUES de comunicación con las demás tareas. Luego de cargar estos datos se bloquea por tiempos variables que dependen de la velocidad vertical estimada.


- **BuzzerTask:**
Tarea que permanece bloqueada hasta que VariometerTask encole un dato. Al despertarse, procesa el dato y vuelve a bloquearse. Tiene distintos modos (vario, on, off) que definen su comportamiento. Estos modos están definidos dentro del dato encolado. Es la tarea de prioridad más alta ya que su tiempo de procesamiento es mínimo y su tiempo de respuesta es importante.


- **DisplayTask:**
Tarea que permanece bloqueada hasta que VariometerTask encole un dato. Al despertarse, procesa el dato y vuelve a bloquearse. Tiene distintos modos (update, clear, on, off) que definen su comportamiento. Estos modos están definidos dentro del dato encolado. Es la tarea de prioridad más baja ya que su tiempo de procesamiento es alto y su tiempo de respuesta NO es demasiado importante.