## Proyecto de Sistemas de Tiempo Real 2026, FaMAF UNC.

### Variómetro + FreeRTOS

#### Componenetes utilizados
- Nucleo H533RE
- BMP280 (sensor barométrico y de temperatura)
- Buzzer Pasivo
- 16x2 LCD Display
- Encoder rotativo (aún no implementado)

#### Estructura de tareas

- **BMP280Task:**
Esta tarea lee el sensor en un intervalo constante, y carga el dato más reciente sobreescribiendo una queue de un único elemento para VariometerTask. Tiene la prioridad más alta.


- **VariometerTask:**
Esta tarea tiene la segunda prioridad más alta. Se despierta al tomar un dato de BMP280Task, le aplica filtros para reducir ruido (Kalman Filter), y encola estos datos procesados a las demás tareas. Luego de cargar estos datos se bloquea hasta recibir un nuevo dato del sensor.


- **BuzzerTask:**
Tarea que permanece bloqueada hasta que VariometerTask encole un dato. Al despertarse, procesa el dato emitiendo los sonidos necesarios y vuelve a bloquearse. Tiene distintos modos (vario, on, off) que definen su comportamiento. Estos modos están definidos como comandos dentro del dato encolado. Tiene la tercer mayor prioridad debe ser la primera en despertarse luego de VariometerTask.


- **DisplayTask:**
Tarea que permanece bloqueada hasta que VariometerTask encole un dato. Al despertarse, procesa el dato y vuelve a bloquearse. Tiene distintos modos (update, clear, on, off) que definen su comportamiento. Estos modos están definidos dentro del dato encolado. Es la tarea de prioridad más baja ya que su tiempo de procesamiento es alto y su tiempo de respuesta NO es demasiado importante.