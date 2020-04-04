/*======================================================================
	Title.	AVR(Atmega128) MCU - DHT11 Sensor Communication
	Author.	K9714 (http://github.com/k9714)
	Date.	2019. 12. 07
	----------------------------------------------------------------
	Desc.
		1. 통신 시작은 18ms 이상의 LOW / 40us 이상의 HIGH(Request)
		2. 이후 DHT11에서는 약 54us의 LOW / 약 80us의 HIGH(Response)
		3. 이후로부터 센서 정보에 대한 데이터가 넘어온다(Packet)
		4. [습도Int][습도Dec][온도Int][습도Dec][CheckSum](8*5=40bit)
		5. LOW ~54us / HIGH ~24us => Data 0
		   LOW ~54us / HIGH ~70us => Data 1
		6. 데이터를 다 보낸 이후 약 54us의 LOW을 보냄(End Packet)


		Ex.
			DHT11_DATA data = {0, 0, 0, 0};
			dht11_read(&data);
======================================================================*/

// 16MHz => 16us speed
F_CPU 16000000UL

// typedef
typedef int dht11_msg;
typedef unsigned char dht11_code;

// Config DHT11 connection
#define DHT11_DDR	DDRD
#define DHT11_PORT	PORTD
#define DHT11_PIN	PIND
#define DHT11_PIN_N	PD0

// TIMEOUT Count (1 check 10us)
#define TIMEOUT_COUNT	100

// MSG CODE
#define SUCCESS		0
#define READY		1
#define PARITY_ERR	2
#define TIMEOUT		255

// DHT11 Struct
typedef struct {
	dht11_code RH_INT;
	dht11_code RH_DEC;
	dht11_code T_INT;
	dht11_code T_DEC;
} DHT11_DATA;


// Read DHT11 response count
dht11_msg dht11_response(dht11_code _lv) {
	int count = 0;
	while (DHT11_PIN & ~(1 << DHT11_PIN_N) == (_lv << DHT11_PIN_N)) {
		if (count++ > TIMEOUT_COUNT) {
			return TIMEOUT;
		}
	}
	return count;
}

dht11_msg dht11_request() {
	// Set DDR OUTPUT & Send LOW signal
	DHT11_DDR |= 1 << DHT11_PIN_N;
	DHT11_PORT = 0;
	_delay_ms(20); // wait >= 18ms (20ms safe)
	// Send HIGH signal
	DHT11_PORT = 1 << DHT11_PIN_N;
	_delay_us(50); // wait >= 40us (16us speed * 3 = 48 < 50us safe)
	// Set DDR INPUT
	DHT11_DDR &= ~(1 << DHT11_PIN_N);
	// 54us LOW check
	if (dht11_response(0) == TIMEOUT) {
		return TIMEOUT;
	}
	// 80us HIGH check
	if (dht11_response(1) == TIMEOUT) {
		return TIMEOUT;
	}
	return READY;
}

dht11_msg dht11_read(DHT11_DATA *_data) {
	dht11_msg req = dht11_request();
	if (req == READY) {
		int i;
		int cycle[80];
		dht11_code data[5];
		// Initialize data 0
		memset(data, 0, 5);
		// Read 40 byte
		for (i = 0; i < 80; i += 2) {
			cycle[i] = dht11_response(0);
			cycle[i + 1] = dht11_response(1);
		}
		// Data
		for (i = 0; i < 40; i++) {
			int low_level = cycle[i * 2];
			int high_level = cycle[i * 2 + 1];
			if ((low_level == TIMEOUT) || (high_level == TIMEOUT)) {
				return TIMEOUT;
			}
			data[i / 8] <<= 1;
			if (high_level > low_level) {
				data[i / 8] |= 1;
			}
		}
		// Parity check
		if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
			_data->RH_INT = data[0];
			_data->RH_DEC = data[1];
			_data->T_INT = data[2];
			_data->T_DEC = data[3];
			return SUCCESS;
		}
		return PARITY_ERR;
	}
	else {
		return req;
	}
}