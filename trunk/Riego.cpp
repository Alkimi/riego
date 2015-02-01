#include <Arduino.h>
#include "Riego.h"
#include "GSM.H"
#include "Menu.h"
#include "zonasDeRiego.h"
#include "miEEPROM.h"

const byte numeroMenusMaximo = 5;
byte numeroMenusActivos = 3;
const byte numeroMaximoDeSubmenus = 4;

//FLASH_STRING_ARRAY(tituloMenu,PSTR("1 Configuraci�n"),PSTR("2 Informaci�n"), PSTR("3  Menu 3"),PSTR("4  Menu 4"),PSTR("5  Menu 5"));
const char tituloMenu[numeroMenusMaximo][17] = { "1 Configuracion", "2 Infomacion",
		"3 Reinicia Zona", "Reinica Manual", "5  Menu 5       " };

byte numeroDeSubmenus[numeroMenusMaximo] = { 4, 2, 4, 1, 4 };

/*FLASH_STRING_ARRAY(tituloSubmenu,PSTR("1.1 xxxxxxxxxx"),PSTR("1.2 Destino SMS"),PSTR("1.3 Fecha"),PSTR("1.4 Hora"),
 PSTR("2.1 Fecha/Hora"),PSTR("2.2 Destino SMS"),PSTR(""),PSTR(""),
 PSTR("3.1 Submenu 1"),PSTR("3.2 Submenu 2"),PSTR(""),PSTR(""),
 PSTR("4.1 Submenu 1"),PSTR(""),PSTR(""),PSTR(""),
 PSTR("5.1 Submenu 1"),PSTR("5.2 Submenu 2"),PSTR("5.3 Submenu 3"),PSTR("5.4 Submenu 4"));
 */
const char tituloSubmenu[numeroMenusMaximo * numeroMaximoDeSubmenus][17] = {
		"1.1 xxxxxxxxxx", "1.2 Destino SMS", "1.3 Fecha", "1.4 Hora",
		"2.1 Fecha/Hora", "2.2 Destino SMS", "", "", "3.1 Zona 1",
		"3.2 Zona 2", "3.3 Zona 3", "3.4 Zona 4", "Pricipal", "", "", "", "5.1 Submenu 1",
		"5.2 Submenu 2", "5.3 Submenu 3", "5.4 Submenu 4" };

unsigned int adc_key_val[5] = { 50, 200, 400, 600, 800 };
char NUM_KEYS = 5;
int key = -1;
boolean luzEncendida = true;
boolean cursorActivo = false;
boolean pantallaEncendida = true;
unsigned long tiempo;
unsigned long tiempo2;
int x = 0;
int y = 0;
String tratarRespuesta;
char* cadena;
char aux[33];
Menu myMenu("", numeroMenusMaximo);
GSM gprs;
zonasDeRiego zonas;
volatile long numeroPulsos=0;
unsigned long totalPulsos=0;
unsigned long anteriorTotalPulsos=0;
boolean regando=false;
boolean funcionamiento=true;

char linea1[16];
char linea2[16];

// Convertimos el valor leido en analogico en un numero de boton pulsado
int get_key(unsigned int input) {
	for (byte k = 0; k < NUM_KEYS; k++) {
		if (input < adc_key_val[k]) {
			return k;
		}
	}
	return -1;
}

void aumentaPulso(void){
	numeroPulsos++;
}

void controlTiempo(void) {
	if (funcionamiento && (millis() - tiempo2) > 5000 ){// cada 20 segundos comprueba el agua consumida
		tiempo2=millis();
		noInterrupts();
		totalPulsos+=numeroPulsos;
		numeroPulsos=0;
		interrupts();
		Serial <<F("Total pulsos: ")<<totalPulsos<<F(" total pulsos anterior: ")<<anteriorTotalPulsos<<endl;
		if (totalPulsos>anteriorTotalPulsos){
			anteriorTotalPulsos=totalPulsos;
			Serial <<F("actualizando anterior")<<endl;

			if (regando){
				//obtenemos el total de litros de todas las zonas
				//convertimos pulso en litros
				//si litros>totalLitros
				//	bloqueamos zonas afectadas
				//	enviamos sms
				//  paramos zonas riego efectadas
				//  actualizamos valor regando

			}else{ // detectado rebenton sin estar regando

				// cerrar principal
				gprs.valvulaPrincipal(CERRAR);
				// enviar sms
				gprs.enviaSMSErrorPrincipal();
				gprs.setProblemaEnZona(PRINCIPAL,true);
				funcionamiento=false;
				x=0;y=0;
				//paso a modo manual
				numeroMenusActivos++;

			}
		}
	}
	if (millis() - tiempo > 15000) { // a los 15 segundos apagamos el display
		pantallaEncendida=false;
		myMenu.noDisplay();
	}
	// Si han pasado mas de 10 segundos apagamos la luz
	if (millis() - tiempo > 10000) {
		pinMode(10, OUTPUT);
		digitalWrite(10, LOW);
		luzEncendida = false;
	}
	// Si han pasado mas de 5 segundos apagamos el cursor
	if (millis() - tiempo > 5000) {
		myMenu.noBlink();
		cursorActivo = false;
	}
}

void estadoProblemaEnZona(byte zona){
	zona=zona-7;
	if (zona==PRINCIPAL){
		strcpy_P(linea1, PSTR("Estado principal:"));
	}else{
		strcpy_P(linea1, PSTR("Estado zona:    "));
		linea1[13]=zona+48;
	}
	myMenu.linea1(linea1);
	if (!gprs.getProblemaEnZona(zona)){
		strcpy_P(linea2, PSTR("ZONA correcta "));
		myMenu.linea2(linea2);
	}else{
		myMenu.blink();
		strcpy_P(linea2, PSTR("   REINICIAR   "));
		myMenu.linea2(linea2);
		//esperamos a tecla select
		bool salida = false;
		do {
			if (lecturaPulsador() == 4) { // Se ha pulsado la tecla de seleccion
				salida = true;
			}
		} while (!salida);
		//guardamos valor en eprom
		if (zona==PRINCIPAL){
			gprs.valvulaPrincipal(ABRIR);
			noInterrupts();
			numeroPulsos=0;
			numeroMenusActivos--;
			totalPulsos=0;
			x=0;y=0;
			anteriorTotalPulsos=0;
			interrupts();
		}else{
			//gprs.valvulaZona(zona);
		}
		gprs.setProblemaEnZona(zona,false);
		funcionamiento=true;
		strcpy_P(linea2, PSTR("ZONA correcta "));
		myMenu.linea2(linea2);
	}
}

void getFechaHora(void) {
	strcpy_P(linea1, PSTR("Fecha: "));
	strcpy_P(linea2, PSTR("Hora : "));
	cadena = gprs.enviaComando(F("AT+CCLK?"));
	linea1[7] = cadena[14];
	linea1[8] = cadena[15];
	linea1[9] = cadena[13];
	linea1[10] = cadena[11];
	linea1[11] = cadena[12];
	linea1[12] = cadena[10];
	linea1[13] = cadena[8];
	linea1[14] = cadena[9];
	cadena += 17;
	cadena[8] = 0;
	strcat(linea2, cadena);
	/*
	 +CCLK: "15/01/11,16:56:39+02"
	 0123456789012345678901234567890
	 0         1         2         3*/
}

bool setFechaHora(byte opcion) {
	bool salida = true;
	sprintf(aux, "AT+CCLK=\"%c%c/%c%c/%c%c,%c%c:%c%c:%c%c+02\"", linea1[13],
			linea1[14], linea1[10], linea1[11], linea1[7], linea1[8], linea2[7],
			linea2[8], linea2[10], linea2[11], linea2[13], linea2[14]);
	cadena = gprs.enviaComando(aux);
	if (cadena == NULL) {
		myMenu.noBlink();
		if (opcion == 2) {
			myMenu.linea2("Fecha erronea");
		} else {
			myMenu.linea2("Hora  erronea");
		}
		delay(1000);
		salida = false;
	}
	return salida;

}

void tratarOpcion(byte x, byte y) {
	byte opcion = (x * numeroMaximoDeSubmenus) + y;
#ifndef RELEASE
	Serial << endl << F("Entrando en tratar opcion") << endl
			<< F("Memoria libre: ") << freeRam() << endl << F("X: ") << x
			<< F(" Y: ") << y << F(" opcion: ") << opcion << endl;
#endif
	myMenu.noBlink();
	switch (opcion) {
	case 1:
		getSMS();
		myMenu.blink();
		cambioNumero(opcion);
		setSMS();
		break;
	case 2:
	case 3:
	case 4:
		getFechaHora();
		myMenu.posicionActual(linea1, linea2);
		if ((opcion == 2) || (opcion == 3)) {
			do {
				myMenu.blink();
				cambioNumero(opcion);
			} while (setFechaHora(opcion) == false);
		}

		break;
	case 5:
		getSMS();
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		estadoProblemaEnZona(opcion);
		break;
	}
	delay(3000);
	myMenu.blink();
#ifndef RELEASE
	Serial << F("saliendo de tratar opcion") << endl << F("Memoria libre: ")
			<< freeRam() << endl;
#endif
}

int lecturaPulsador(void){
	int adc_key_inNumero;
	int keyNumero = -1;

	adc_key_inNumero = analogRead(0);    // Leemos el valor de la pulsacion
	keyNumero = get_key(adc_key_inNumero);    // Obtenemos el boton pulsado
	if (keyNumero != -1) {  // if keypress is detected
		delay(100);  // Espera para evitar los rebotes de las pulsaciones
		adc_key_inNumero = analogRead(0); // Leemos el valor de la pulsacion
		keyNumero = get_key(adc_key_inNumero); // Obtenemos el boton pulsado
		if (keyNumero != -1) {
			if (!luzEncendida) {
				pinMode(10, INPUT);
				luzEncendida = true;
				cursorActivo = true;
				pantallaEncendida=true;
				myMenu.blink(); // Mostramos el cursor parpadeando
				myMenu.display(); //encendemos la pantalla
			}
			return keyNumero;
		}
	}
	return -1;
}

void cambioNumero(byte opcion) {
	bool salida = false;
	char caracter;

	byte xmax, xmin;
	byte xNumero;
	byte columna;
	int keyNumero;

	char * linea;

	xmax = 14;
	xmin = 7;
	linea = linea2;
	columna = 0;

	switch (opcion) {
	case 1:
		xmin = 3;
		xmax = 11;
		columna = 1;
		break;
	case 2:
		myMenu.borraLinea2();
		linea = linea1;
		break;
	case 3:
		myMenu.borraLinea1();
		myMenu.borraLinea2();
		myMenu.linea1(linea);
		break;
	}
	myMenu.SetCursor(xmin, columna);
	caracter = linea[xmin];
	xNumero = xmin;
	do {
		keyNumero=lecturaPulsador();
		if (keyNumero >= 0 && keyNumero < NUM_KEYS) {
			myMenu.SetCursor(xNumero, columna);
#ifndef RELEASE
			Serial << F("entrada Tecla pulsada: ") << keyNumero << endl
					<< F(" x= ") << xNumero << F(" caracter: ")
					<< caracter << F(" linea: ") << linea << endl;
#endif

		if (keyNumero == 0) {  // Se ha pulsado la tecla derecha
			xNumero++;
			if ((opcion > 1) && ((xNumero == 9) || (xNumero == 12)))
				xNumero++;
			if (xNumero > xmax)
				xNumero = xmin;
			caracter = linea[xNumero];
		}
		if (keyNumero == 1) {   // Se ha pulsado la tecla arriba
			caracter--;
			if (caracter < '0')
				caracter = '9';
			linea[xNumero] = caracter;
			myMenu.write(caracter);
		}
		if (keyNumero == 2) {  // Se ha pulsado la tecla abajo
			caracter++;
			if (caracter > '9')
				caracter = '0';
			linea[xNumero] = caracter;
			myMenu.write(caracter);
		}

		if (keyNumero == 3) {  // Se ha pulsado la tecla izquierda
			xNumero--;
			if ((opcion > 1) && ((xNumero == 9) || (xNumero == 12)))
				xNumero--;
			if (xNumero < xmin)
				xNumero = xmax;
			caracter = linea[xNumero];
		}
		if (keyNumero == 4) { // Se ha pulsado la tecla de seleccion
			salida = true;
		}
		myMenu.SetCursor(xNumero, columna);
#ifndef RELEASE
		Serial << F("SALIDA Tecla pulsada: ") << keyNumero << endl
				<< F(" x= ") << xNumero << F(" caracter: ")
				<< caracter << F(" linea: ") << linea << endl;
#endif
		}
	} while (!salida);
}

void setSMS(void) {
	EEPROM.escrituraEeprom16(0, linea2);
}

void getSMS(void) {
	myMenu.noBlink();
	EEPROM.lecturaEeprom16(0, linea2);
	if (pantallaEncendida){
		strcpy_P(linea1, PSTR("Destino SMS:"));
		myMenu.posicionActual(linea1, linea2);
	}
}

void setup() {
	Serial.begin(4800);
#ifndef RELEASE_FINAL
	Serial << F("Memoria libre: ") << freeRam() << endl
			<< F("------------------------") << endl;
	Serial << F("Riego Total V ") << RIEGO_VERSION << endl << F("Menu V ")
			<< myMenu.libVer() << endl << F("GPRS V ") << gprs.libVer() << endl;
#endif
	myMenu.inicia(gprs.libVer());
	myMenu.posicionActual(tituloMenu[x],
			tituloSubmenu[(x * numeroMaximoDeSubmenus) + y]);
#ifndef RELEASE_FINAL
	//zonas.imprimirZonas();
#endif
	//gprs.inicializaAlarmas(&zonas);
	tiempo = millis();
	attachInterrupt(0, aumentaPulso, FALLING); // interrupcion 0 habilitada en el pin 2 sobre el metodo aumentaPulso en el flanco de bajada

}

void loop() {
	comandoGPRS();
	controlTiempo();
	key=lecturaPulsador();
	if (key != -1) {
		Serial <<F("valor x: ")<<x<<F(" valor y: ")<<y<<endl;
		tiempo = millis();
		if (key == 0) {  // Se ha pulsado la tecla derecha
			x++;
			if (x > numeroMenusActivos - 1)
				x = 0;
			y = 0;
		}
		if (key == 1) {   // Se ha pulsado la tecla arriba
			y--;
			if (y < 0 )
				y = numeroDeSubmenus[x] - 1;
		}
		if (key == 2) {  // Se ha pulsado la tecla abajo
			y++;
			if (y > numeroDeSubmenus[x] - 1)
				y = 0;
		}

		if (key == 3) {  // Se ha pulsado la tecla izquierda
			x--;
			if (x < 0)
				x = numeroMenusActivos - 1;
			y = 0;
		}
		if (key == 4) {  // Se ha pulsado la tecla de seleccion
			tratarOpcion(x, y);
			tiempo = millis();
			pinMode(10, INPUT);
			luzEncendida = true;
			pantallaEncendida=true;
			myMenu.display(); //encendemos la pantalla
			cursorActivo = true;
			myMenu.blink(); // Mostramos el cursor parpadeando
		}
		myMenu.posicionActual(tituloMenu[x],
				tituloSubmenu[(x * numeroMaximoDeSubmenus) + y]);
	}
}

void tratarRespuestaGprs() {
	//alama
#ifndef RELEASE
	Serial << F("dentro de tratarRespuesta GPRS") << endl << F("Cadena recivida: ") << tratarRespuesta << endl;

	/*for (int i = 0;i<tratarRespuesta.length();i++){
		Serial << F("ASCII:  ") << (byte)tratarRespuesta.charAt(i) << F(" caracter: ") << tratarRespuesta.charAt(i)<< F(" Indice: ")<< i << endl;


	}*/
#endif
	tratarRespuesta=tratarRespuesta.substring(2,tratarRespuesta.length()-2);
	if (tratarRespuesta.startsWith(PSTR("+CALV:"))) {
#ifndef RELEASE
		Serial << F("dentro de CALV") << endl;
#endif

		byte alarma = ((byte) tratarRespuesta.charAt(7)) - 48;
#ifndef RELEASE_FINAL
		zonas.imprimirZonas();
#endif
		if (!gprs.getProblemaEnZona(alarma)){
			if (zonas.getEstadoPrimeraVez(alarma) == false) //salta la alarma se establece la duracion
			{
				zonas.setEstadoPrimeraVez(alarma);  //cambiamos es estado
				gprs.iniciarRiegoZona(alarma);
				gprs.establecerHoraFin(zonas.getZonaDeRiego(alarma));
			} else    // salta la alarma porque ha terminado el tiempo de riego
			{
				zonas.setEstadoPrimeraVez(alarma);    //cambiamos es estado
				gprs.pararRiegoZona(alarma);
				gprs.establecerHoraInicio(zonas.getZonaDeRiego(alarma));
			}
		}
#ifndef RELEASE_FINAL
		zonas.imprimirZonas();
#endif
#ifndef RELEASE
		Serial << F("fuera de CALV") << endl;
#endif
	}
#ifndef RELEASE
	Serial << F("fuera de tratarRespuesta GPRS") << endl;
#endif

}

void comandoGPRS(void) {
#ifndef SIMPLE
	if (gprs.available()) // if date is comming from softwareserial port ==> data is comming from gprs shield
	{
		tratarRespuesta = gprs.readString();
#ifndef RELEASE
		Serial << F("leyendo del gprs") << endl;
#endif
		tratarRespuestaGprs();
#ifndef RELEASE
		Serial << tratarRespuesta << endl;
#endif
		//Serial.println(tratarRespuesta.toCharArray());
	}
#ifndef RELEASE_FINAL
	if (Serial.available()) // if data is available on hardwareserial port ==> data is comming from PC or notebook
	{
		tratarRespuesta = Serial.readString();
		if (tratarRespuestaSerial()) {
#ifndef RELEASE
			Serial << F("escribiendo en gprs") << endl << tratarRespuesta
					<< endl;
#endif

			gprs.enviaComando(tratarRespuesta);       // write it to the GPRS shield
		}
	}
#endif

#else
	if (gprs.available()){ // if date is comming from softwareserial port ==> data is comming from gprs shield
		Serial << F("leyendo del gprs") << endl;
		Serial << gprs.readString()<<endl;
	}
	if (Serial.available()) {// if data is available on hardwareserial port ==> data is comming from PC or notebook
		Serial << F("escribiendo en gprs") << endl;
		gprs.println(Serial.readString());
	}
#endif
}

#ifndef RELEASE_FINAL
/////////////////////////////////////////////funciones debub
bool tratarRespuestaSerial() {
	bool salidaRespuesta = true;
#ifndef RELEASE
	Serial << F("dentro de tratarRespuesta Serial") << endl;
#endif
	if (tratarRespuesta.startsWith("ER:")){
		for (int i = 0;i<1024;i++){
			EEPROM.write(i,'\x0');
		}
	}
	if (tratarRespuesta.startsWith("E:")) {
#ifndef RELEASE
		Serial << F("dentro de Escritura:") << endl;
#endif
		byte pos = (tratarRespuesta.charAt(2)-48);
#ifndef RELEASE
		Serial << F("valor de pos= ") << pos << endl;
#endif
		pos = pos * 16;
		for (int i = 4; i < tratarRespuesta.length(); i++) {
			EEPROM.write(pos, (byte) tratarRespuesta.charAt(i));
			Serial <<F("posicion: ")<<pos<<F(" valor: ")<<(byte) tratarRespuesta.charAt(i)<< F(" caracter: ") <<tratarRespuesta.charAt(i) <<endl;
			pos++;
			i++;
		}
#ifndef RELEASE
		Serial << F("fuera de Escritura:") << endl;
#endif
		salidaRespuesta = false;
	}
	if (tratarRespuesta.startsWith("L:")) {
		byte pos = (tratarRespuesta.charAt(2)-48);
#ifndef RELEASE
		Serial << F("dentro de Lectura:") << endl;
#endif
		leerEEPROM(pos);
#ifndef RELEASE
		Serial << F("fuera de lectura:") << endl;
#endif
		salidaRespuesta = false;
	}
#ifndef RELEASE
	Serial << F("fuera de tratarRespuesta Serial") << endl;
#endif
	return salidaRespuesta;

}

int freeRam() {
#ifndef DEBUG_PROCESS
	extern int __heap_start, *__brkval;
	int v;
	return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
#else
	return 0;
#endif
}

void leerEEPROM(byte pos) {
#ifndef RELEASE
	byte valor;
	Serial << endl
			<< F(
					"0                   1                   2                   3")
			<< endl;
	Serial
			<< F(
					"0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1")
			<< endl;
	Serial
			<< F(
					"---------------------------------------------------------------")
			<< endl;
	int limite;
	if (pos==0)
		limite =320;
	else
		limite = pos*16;
	for (int i = 0; i < limite; i++) {
		valor = EEPROM.read(i);
		if (i > 0 && i % 32 == 0) {
			Serial << endl;
		}
		Serial << valor << F(" ");

	}
	Serial << endl;
#endif
}
#endif
