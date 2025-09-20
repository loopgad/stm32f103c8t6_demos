#include "my_callback.h"

/* Protocol Constants */
#define HEAD1 0xAA    /**< First header byte indicating packet start */
#define HEAD2 0xAF    /**< Second header byte indicating packet start */
#define TAIL1 0xFA    /**< First tail byte indicating packet end */
#define TAIL2 0xFF    /**< Second tail byte indicating packet end */

/**
 * @brief UART parsing state enumeration
 * @note Defines all possible states for the state machine parser[7,8](@ref)
 */
typedef enum {
    STATE_WAIT_HEAD1 = 0,  /**< Waiting for first header byte */
    STATE_WAIT_HEAD2,      /**< Waiting for second header byte */
    STATE_RECEIVE_DATA,    /**< Receiving data bytes */
    STATE_WAIT_TAIL1,      /**< Waiting for first tail byte */
    STATE_WAIT_TAIL2       /**< Waiting for second tail byte */
} uart_parse_state_t;

#define MAX_LENGTH 10       /**< Maximum number of messages to store */
#define MAX_ARR 14400-1      /**< Maximum value for PWM counter */

/**
 * @brief Union for easy conversion between bytes and float
 * @note Allows accessing the same data as both raw bytes and float value[8](@ref)
 */
typedef union {
    uint8_t buf[4];  /**< Raw byte representation */
    float data;       /**< Float value representation */
} send_msg;

uint8_t usart3_buf[1] = {0};    /**< Buffer for UART received byte */
uint16_t pwm_ccr = 0;           /**< PWM capture/compare value */
send_msg my_msg_tx[MAX_LENGTH]; /**< Transmission messages buffer */
send_msg my_msg_rx[MAX_LENGTH]; /**< Reception messages buffer */

static uint8_t cnt = 0; /**< General purpose counter */

/**
 * @brief UART3 packet parser state machine (single function implementation)
 * @param rx_byte Pointer to the incoming byte from UART
 * @retval 1 = packet complete, 0 = packet in progress, -1 = error
 * @note Implements state machine for AA AF [data] FA FF packet format[7,8](@ref)
 * 
 * This function processes incoming UART bytes using a state machine approach[6,7](@ref),
 * which is efficient for parsing variable-length protocols[6](@ref). The state machine
 * transitions through different states as it receives the header, data, and tail bytes[8](@ref).
 */
int8_t u3_packet_parser(uint8_t *rx_byte)
{    
    uint8_t received_byte = *rx_byte;
    static uint8_t packet_count = 0; /**< Circular buffer index for received packets */
    
    // State machine states[7,8](@ref)
    static enum {
        STATE_WAIT_HEAD1 = 0,  // Waiting for first header byte
        STATE_WAIT_HEAD2,      // Waiting for second header byte  
        STATE_RECEIVE_DATA,    // Receiving data bytes
        STATE_WAIT_TAIL1,      // Waiting for first tail byte
        STATE_WAIT_TAIL2       // Waiting for second tail byte
    } parse_state = STATE_WAIT_HEAD1;
    
    static uint8_t data_index = 0;     /**< Current data byte index */
    static uint8_t packet_buffer[8];   /**< Complete packet buffer */
    
    // State machine processing[7,8](@ref)
    switch (parse_state) {
        case STATE_WAIT_HEAD1:
            // Wait for first header byte
            if (received_byte == HEAD1) {
                packet_buffer[0] = received_byte;
                parse_state = STATE_WAIT_HEAD2; // Move to next state
            }
            break;
            
        case STATE_WAIT_HEAD2:
            // Wait for second header byte
            if (received_byte == HEAD2) {
                packet_buffer[1] = received_byte;
                data_index = 0;
                parse_state = STATE_RECEIVE_DATA; // Move to data reception state
            } else {
                parse_state = STATE_WAIT_HEAD1;  // Reset on header mismatch
            }
            break;
            
        case STATE_RECEIVE_DATA:
            // Store data bytes (positions 2-5 in packet buffer)
            if (data_index < 4) {
                packet_buffer[2 + data_index] = received_byte;
                data_index++;
            }
            
            // Check if all 4 data bytes received
            if (data_index >= 4) {
                parse_state = STATE_WAIT_TAIL1; // Move to tail verification
            }
            break;
            
        case STATE_WAIT_TAIL1:
            // Verify first tail byte
            if (received_byte == TAIL1) {
                packet_buffer[6] = received_byte;
                parse_state = STATE_WAIT_TAIL2; // Move to final tail verification
            } else {
                parse_state = STATE_WAIT_HEAD1;  // Reset on tail mismatch
                return -1; // Return error
            }
            break;
            
        case STATE_WAIT_TAIL2:
            // Verify second tail byte and complete packet processing
            if (received_byte == TAIL2) {
                packet_buffer[7] = received_byte;
                
                // Store received data in message array (circular buffer)
                if (packet_count < 10) {
                    for (int i = 0; i < 4; i++) {
                        my_msg_rx[packet_count].buf[i] = packet_buffer[2 + i];
                    }
                    packet_count = (packet_count + 1) % 10;  // Circular buffer
                }
                
                parse_state = STATE_WAIT_HEAD1;  // Reset for next packet
                return 1;  // Packet complete
            } else {
                parse_state = STATE_WAIT_HEAD1;  // Reset on tail mismatch
                return -1; // Return error
            }
            break;
            
        default:
            parse_state = STATE_WAIT_HEAD1;  // Reset on unknown state
            break;
    }
    
    return 0;  // Packet reception in progress
}

/**
 * @brief Send data via UART3 with protocol formatting
 * @param huart Pointer to UART handle structure
 * @param buf Float data to be sent
 * @note Formats data into protocol packet: HEAD1 HEAD2 DATA TAIL1 TAIL2[6](@ref)
 */
static void send_u3_data(UART_HandleTypeDef *huart, float buf)
{
    my_msg_tx[0].data = buf; // Store float data in union
    
    uint8_t tmp_buf[8] = {0}; // Temporary buffer for packet formatting
    tmp_buf[0] = HEAD1;       // First header byte
    tmp_buf[1] = HEAD2;       // Second header byte
    tmp_buf[6] = TAIL1;       // First tail byte
    tmp_buf[7] = TAIL2;       // Second tail byte
    
    // Copy data bytes to packet
    for(int i = 0; i < 4; i++){
        tmp_buf[2 + i] = my_msg_tx[0].buf[i];
    }
    
    // Transmit complete packet
    HAL_UART_Transmit(huart, tmp_buf, 8, HAL_MAX_DELAY);
}

/**
 * @brief Limit PWM capture/compare value to valid range
 * @param ccr Pointer to PWM capture/compare value
 * @note Prevents CCR value from exceeding maximum allowed value[5](@ref)
 */
static void limit_ccr(uint16_t *ccr){
    *ccr = *ccr > MAX_ARR ? MAX_ARR : *ccr;
}

/**
 * @brief TIM period elapsed callback function
 * @param htim Pointer to TIM handle structure
 * @note Called when timer period completes[8](@ref)
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // Process TIM3 interrupts only
    if (htim->Instance == TIM3)
    {
        // Send data every 10 timer periods
        if(cnt++ >= 10){
            send_u3_data(&huart3, 1.2f); // Send sample data
            cnt = 0; // Reset counter
        }
        
        // Limit and update PWM value
        limit_ccr(&pwm_ccr);
        __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, pwm_ccr); // set pwm with your ccr value(pluse wide)
    }
}

/**
 * @brief UART reception complete callback function
 * @param huart Pointer to UART handle structure
 * @note Called when UART receives data[8](@ref)
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    // Process USART3 interrupts only
    if (huart->Instance == USART3)
    {
				u3_packet_parser(usart3_buf); // Parse received byte	
				HAL_UART_Receive_IT(&huart3, usart3_buf, 1); // Restart UART reception
    }
}