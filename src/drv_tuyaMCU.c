#include "new_common.h"
#include "new_pins.h"
#include "new_cfg.h"
#include "new_cmd.h"
#include "logging/logging.h"
#include "drv_tuyaMCU.h"


#if PLATFORM_BK7231T | PLATFORM_BK7231N
#include "../../beken378/func/user_driver/BkDriverUart.h"
#endif

#define TUYA_CMD_HEARTBEAT     0x00
#define TUYA_CMD_QUERY_PRODUCT 0x01
#define TUYA_CMD_MCU_CONF      0x02
#define TUYA_CMD_WIFI_STATE    0x03
#define TUYA_CMD_WIFI_RESET    0x04
#define TUYA_CMD_WIFI_SELECT   0x05
#define TUYA_CMD_SET_DP        0x06
#define TUYA_CMD_STATE         0x07
#define TUYA_CMD_QUERY_STATE   0x08
#define TUYA_CMD_SET_TIME      0x1C

void TuyaMCU_RunFrame();

// from http_fns.  should move to a utils file.
extern unsigned char hexbyte( const char* hex );

#if PLATFORM_BK7231T | PLATFORM_BK7231N
	// from uart_bk.c
	extern void bk_send_byte(UINT8 uport, UINT8 data);
#elif WINDOWS
#else
#endif


const char *TuyaMCU_GetCommandTypeLabel(int t) {
	if(t == TUYA_CMD_HEARTBEAT)
		return "Hearbeat";
	if(t == TUYA_CMD_QUERY_PRODUCT)
		return "QueryProductInformation";
	if(t == TUYA_CMD_MCU_CONF)
		return "MCUconf";
	if(t == TUYA_CMD_WIFI_STATE)
		return "WiFiState";
	if(t == TUYA_CMD_WIFI_RESET)
		return "WiFiReset";
	if(t == TUYA_CMD_WIFI_SELECT)
		return "WiFiSelect";
	if(t == TUYA_CMD_SET_DP)
		return "SetDP";
	if(t == TUYA_CMD_STATE)
		return "State";
	if(t == TUYA_CMD_QUERY_STATE)
		return "QueryState";
	if(t == TUYA_CMD_SET_TIME)
		return "SetTime";
	return "Unknown";
}
typedef struct rtcc_s {
	uint8_t       second;
	uint8_t       minute;
	uint8_t       hour;
	uint8_t       day_of_week;               // sunday is day 1
	uint8_t       day_of_month;
	uint8_t       month;
	char          name_of_month[4];
	uint16_t      day_of_year;
	uint16_t      year;
	uint32_t      days;
	uint32_t      valid;
} rtcc_t;

static byte *g_recvBuf = 0;
static int g_recvBufSize = 0;
static int g_recvBufIn = 0;
static int g_recvBufOut = 0;

void UART_InitReceiveRingBuffer(int size){
	if(g_recvBuf!=0)
		free(g_recvBuf);
	g_recvBuf = (byte*)malloc(size);
	memset(g_recvBuf,0,size);
	g_recvBufSize = size;
	g_recvBufIn = 0;
}
int UART_GetDataSize()
{
    int remain_buf_size = 0;

    if(g_recvBufIn >= g_recvBufOut) {
        remain_buf_size = g_recvBufIn - g_recvBufOut;
    }else {
        remain_buf_size = g_recvBufIn + g_recvBufSize - g_recvBufOut;
    }
    
    return remain_buf_size;
}
byte UART_GetNextByte(int index) {
	int realIndex = g_recvBufOut + index;
	if(realIndex > g_recvBufSize)
		realIndex -= g_recvBufSize;

	return g_recvBuf[realIndex];
}
void UART_ConsumeBytes(int idx) {
	g_recvBufOut += idx;
	if(g_recvBufOut > g_recvBufSize)
		g_recvBufOut -= g_recvBufSize;
}
// header version command lenght data checksum
// 55AA		00		00		0000   xx	00

#define MIN_TUYAMCU_PACKET_SIZE (2+1+1+2+1)
int UART_TryToGetNextTuyaPacket(byte *out, int maxSize) {
	int cs;
	int len, i;
	int c_garbage_consumed = 0;
	byte a, b, version, command, lena, lenb;
	
	cs = UART_GetDataSize();

	if(cs < MIN_TUYAMCU_PACKET_SIZE) {
		return 0;
	}
	// skip garbage data (should not happen)
	while(cs > 0) {
		a = UART_GetNextByte(0);
		b = UART_GetNextByte(1);
		if(a != 0x55 || b != 0xAA) {
			UART_ConsumeBytes(1);
			c_garbage_consumed++;
			cs--;
		} else {
			break;
		}
	}
	if(c_garbage_consumed > 0){
		printf("Consumed %i unwanted non-header byte in Tuya MCU buffer\n", c_garbage_consumed);
	}
	if(cs < MIN_TUYAMCU_PACKET_SIZE) {
		return 0;
	}
	a = UART_GetNextByte(0);
	b = UART_GetNextByte(1);
	if(a != 0x55 || b != 0xAA) {
		return 0;
	}
	version = UART_GetNextByte(2);
	command = UART_GetNextByte(3);
	lena = UART_GetNextByte(4); // hi
	lenb = UART_GetNextByte(5); // lo
	len = lenb | lena >> 8;
	// now check if we have received whole packet
	len += 2 + 1 + 1 + 2 + 1; // header 2 bytes, version, command, lenght, chekcusm
	if(cs >= len) {
		for(i = 0; i < len; i++) {
			out[i] = UART_GetNextByte(i);
		}
		// consume whole packet (but don't touch next one, if any)
		UART_ConsumeBytes(len);
		return len;
	}
	return 0;
}
void UART_AppendByteToCircularBuffer(int rc) {
    if(UART_GetDataSize() < (g_recvBufSize-1)) 
    {
        g_recvBuf[g_recvBufIn++] = rc;
        if(g_recvBufIn >= g_recvBufSize){
            g_recvBufIn = 0;
        }
   }
}
#if PLATFORM_BK7231T | PLATFORM_BK7231N
void test_ty_read_uart_data_to_buffer(int port, void* param)
{
    int rc = 0;
    
    while((rc = uart_read_byte(port)) != -1)
    {
		UART_AppendByteToCircularBuffer(rc);
    }

}
#endif

void TuyaMCU_Bridge_InitUART(int baud) {
#if PLATFORM_BK7231T | PLATFORM_BK7231N
	bk_uart_config_t config;

    config.baud_rate = 9600;
    config.data_width = 0x03;
    config.parity = 0;    //0:no parity,1:odd,2:even
    config.stop_bits = 0;   //0:1bit,1:2bit
    config.flow_control = 0;   //FLOW_CTRL_DISABLED
    config.flags = 0;

    bk_uart_initialize(0, &config, NULL);
    bk_uart_set_rx_callback(0, test_ty_read_uart_data_to_buffer, NULL);
#else


#endif
}



void TuyaMCU_Bridge_SendUARTByte(byte b) {
#if PLATFORM_BK7231T | PLATFORM_BK7231N
	bk_send_byte(0, b);
#elif WINDOWS
	// STUB - for testing
    printf("%02X", b);
#else


#endif
}

// append header, len, everything, checksum
void TuyaMCU_SendCommandWithData(byte cmdType, byte *data, int payload_len) {
	int i;

	byte check_sum = (0xFF + cmdType + (payload_len >> 8) + (payload_len & 0xFF));
	TuyaMCU_Bridge_InitUART(9600);
	TuyaMCU_Bridge_SendUARTByte(0x55);
	TuyaMCU_Bridge_SendUARTByte(0xAA);
	TuyaMCU_Bridge_SendUARTByte(0x00);         // version 00
	TuyaMCU_Bridge_SendUARTByte(cmdType);         // version 00
	TuyaMCU_Bridge_SendUARTByte(payload_len >> 8);      // following data length (Hi)
	TuyaMCU_Bridge_SendUARTByte(payload_len & 0xFF);    // following data length (Lo)
	for(i = 0; i < payload_len; i++) {
		byte b = data[i];
		check_sum += b;
		TuyaMCU_Bridge_SendUARTByte(b);
	}
	TuyaMCU_Bridge_SendUARTByte(check_sum);
}

void TuyaMCU_Send_SetTime(rtcc_t *pTime) {
	byte payload_buffer[8];
	byte tuya_day_of_week;

	if (pTime->day_of_week == 1) {
		tuya_day_of_week = 7;
	} else {
		tuya_day_of_week = pTime->day_of_week-1;
	}

	payload_buffer[0] = 0x01;
	payload_buffer[1] = pTime->year % 100;
	payload_buffer[2] = pTime->month;
	payload_buffer[3] = pTime->day_of_month;
	payload_buffer[4] = pTime->hour;
	payload_buffer[5] = pTime->minute;
	payload_buffer[6] = pTime->second;
	payload_buffer[7] = tuya_day_of_week; //1 for Monday in TUYA Doc

	TuyaMCU_SendCommandWithData(TUYA_CMD_SET_TIME, payload_buffer, 8);
}

int TuyaMCU_Send_Hex(const void *context, const char *cmd, const char *args) {
	//const char *args = CMD_GetArg(1);
	if(!(*args)) {
		printf("TuyaMCU_Send_Hex: requires 1 argument (hex string, like FFAABB00CCDD\n");
		return -1;
	}
	while(*args) {
		byte b;
		b = hexbyte(args);
		
		TuyaMCU_Bridge_SendUARTByte(b);

		args += 2;
	}
	return 1;
}

int TuyaMCU_Send_SetTime_Example(const void *context, const char *cmd, const char *args) {
	rtcc_t testTime;

	testTime.year = 2012;
	testTime.month = 7;
	testTime.day_of_month = 15;
	testTime.day_of_week = 4;
	testTime.hour = 6;
	testTime.minute = 54;
	testTime.second = 32;

	TuyaMCU_Send_SetTime(&testTime);
	return 1;
}

void TuyaMCU_Send(byte *data, int size) {
	int i;
    unsigned char check_sum;

	check_sum = 0;
	for(i = 0; i < size; i++) {
		byte b = data[i];
		check_sum += b;
		TuyaMCU_Bridge_SendUARTByte(b);
	}
	TuyaMCU_Bridge_SendUARTByte(check_sum);

	printf("\nWe sent %i bytes to Tuya MCU\n",size+1);
}
void TuyaMCU_Init()
{
	TuyaMCU_Bridge_InitUART(9600);
	UART_InitReceiveRingBuffer(256);
	CMD_RegisterCommand("tuyaMcu_testSendTime","",TuyaMCU_Send_SetTime_Example, "Sends a example date by TuyaMCU to clock/callendar MCU", NULL);
	CMD_RegisterCommand("uartSendHex","",TuyaMCU_Send_Hex, "Sends raw data by TuyaMCU UART, you must write whole packet with checksum yourself", NULL);
	///CMD_RegisterCommand("tuyaMcu_sendSimple","",TuyaMCU_Send_Simple, "Appends a 0x55 0xAA header to a data, append a checksum at end and send");

}


void TuyaMCU_ProcessIncoming(const byte *data, int len) {
	int checkLen;
	int i;
	byte checkCheckSum;
	byte cmd;
	if(data[0] != 0x55 || data[1] != 0xAA) {
		printf("TuyaMCU_ProcessIncoming: discarding packet with bad ident and len %i\n",len);
		return;
	}
	checkLen = data[5] | data[4] >> 8;
	checkLen = checkLen + 2 + 1 + 1 + 2 + 1;
	if(checkLen != len) {
		printf("TuyaMCU_ProcessIncoming: discarding packet bad expected len, expected %i and got len %i\n",checkLen,len);
		return;
	}
	checkCheckSum = 0;
	for(i = 0; i < len-1; i++) {
		checkCheckSum += data[i];
	}
	if(checkCheckSum != data[len-1]) {
		printf("TuyaMCU_ProcessIncoming: discarding packet bad expected checksum, expected %i and got checksum %i\n",(int)data[len-1],(int)checkCheckSum);
		return;
	}
	cmd = data[3];
	printf("TuyaMCU_ProcessIncoming: processing command %i (%s) with %i bytes\n",cmd,TuyaMCU_GetCommandTypeLabel(cmd),len);

}
void TuyaMCU_RunFrame() {
	byte data[128];
	char buffer_for_log[256];
	char buffer2[4];
	int len, i;

	//printf("UART ring buffer state: %i %i\n",g_recvBufIn,g_recvBufOut);

	len = UART_TryToGetNextTuyaPacket(data,sizeof(data));
	if(len > 0) {
		//printf("TUYAMCU received: ");
		buffer_for_log[0] = 0;
		for(i = 0; i < len; i++) {
			//printf("%02X ",data[i]);
			sprintf(buffer2,"%02X ",data[i]);
			strcat_safe(buffer_for_log,buffer2,sizeof(buffer_for_log));
		}
		//printf(buffer_for_log);
		//printf("\n");
		printf("TUYAMCU received: %s\n", buffer_for_log);
		TuyaMCU_ProcessIncoming(data,len);
	}
}
