/*

Copyright 2015-2021 Igor Petrovic

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#ifdef USE_UART

#include "board/Board.h"
#include "board/Internal.h"
#include "core/src/arch/avr/Atomic.h"
#include "core/src/arch/avr/UART.h"
#include "MCU.h"

namespace Board
{
    namespace detail
    {
        namespace UART
        {
            namespace ll
            {
                void enableDataEmptyInt(uint8_t channel)
                {
                    switch (channel)
                    {
                    case 0:
                        UCSRB_0 |= (1 << UDRIE_0);
                        break;

#ifdef UCSRB_1
                    case 1:
                        UCSRB_1 |= (1 << UDRIE_1);
                        break;
#endif

                    default:
                        break;
                    }
                }

                void disableDataEmptyInt(uint8_t channel)
                {
                    switch (channel)
                    {
                    case 0:
                        UCSRB_0 &= ~(1 << UDRIE_0);
                        break;

#ifdef UCSRB_1
                    case 1:
                        UCSRB_1 &= ~(1 << UDRIE_1);
                        break;
#endif

                    default:
                        break;
                    }
                }

                bool deInit(uint8_t channel)
                {
                    if (channel >= MAX_UART_INTERFACES)
                        return false;

                    ATOMIC_SECTION
                    {
                        switch (channel)
                        {
                        case 0:
                            UCSRA_0 = 0;
                            UCSRB_0 = 0;
                            UCSRC_0 = 0;
                            UBRR_0  = 0;
                            break;

#ifdef UCSRB_1
                        case 1:
                            UCSRA_1 = 0;
                            UCSRB_1 = 0;
                            UCSRC_1 = 0;
                            UBRR_1  = 0;
                            break;
#endif

                        default:
                            break;
                        }
                    }

                    return true;
                }

                bool init(uint8_t channel, uint32_t baudRate)
                {
                    if (channel >= MAX_UART_INTERFACES)
                        return false;

                    if (!deInit(channel))
                        return false;

                    int32_t baud_count = ((F_CPU / 8) + (baudRate / 2)) / baudRate;

                    if ((baud_count & 1) && baud_count <= 4096)
                    {
                        switch (channel)
                        {
                        case 0:
                            UCSRA_0 = (1 << U2X_0);    //double speed uart
                            UBRR_0  = baud_count - 1;
                            break;

#ifdef UCSRA_1
                        case 1:
                            UCSRA_1 = (1 << U2X_1);    //double speed uart
                            UBRR_1  = baud_count - 1;
                            break;
#endif

                        default:
                            break;
                        }
                    }
                    else
                    {
                        switch (channel)
                        {
                        case 0:
                            UCSRA_0 = 0;
                            UBRR_0  = (baud_count >> 1) - 1;
                            break;

#ifdef UCSRA_1
                        case 1:
                            UCSRA_1 = 0;
                            UBRR_1  = (baud_count >> 1) - 1;
                            break;
#endif

                        default:
                            break;
                        }
                    }

                    //8 bit, no parity, 1 stop bit
                    //enable receiver, transmitter and receive interrupt
                    switch (channel)
                    {
                    case 0:
                        UCSRC_0 = (1 << UCSZ1_0) | (1 << UCSZ0_0);
                        UCSRB_0 = (1 << RXEN_0) | (1 << TXEN_0) | (1 << RXCIE_0) | (1 << TXCIE_0);
                        break;

#ifdef UCSRC_1
                    case 1:
                        UCSRC_1 = (1 << UCSZ1_1) | (1 << UCSZ0_1);
                        UCSRB_1 = (1 << RXEN_1) | (1 << TXEN_1) | (1 << RXCIE_1) | (1 << TXCIE_1);
                        break;
#endif

                    default:
                        break;
                    }

                    return true;
                }
            }    // namespace ll
        }        // namespace UART
    }            // namespace detail
}    // namespace Board

/// ISR used to store incoming data from UART to buffer.

ISR(USART_RX_vect_0)
{
    uint8_t data = UDR_0;
    Board::detail::UART::storeIncomingData(0, data);
}

#ifdef UDR_1
ISR(USART_RX_vect_1)
{
    uint8_t data = UDR_1;
    Board::detail::UART::storeIncomingData(1, data);
}
#endif

///

/// ISR used to write outgoing data in buffer to UART.

ISR(USART_UDRE_vect_0)
{
    uint8_t data;
    size_t  dummy;

    if (Board::detail::UART::getNextByteToSend(0, data, dummy))
        UDR_0 = data;
}

#ifdef UDR_1
ISR(USART_UDRE_vect_1)
{
    uint8_t data;
    size_t  dummy;

    if (Board::detail::UART::getNextByteToSend(1, data, dummy))
        UDR_1 = data;
}
#endif

///

/// ISR fired once the UART transmission is complete.

ISR(USART_TX_vect_0)
{
    Board::detail::UART::indicateTxComplete(0);
}

#ifdef UDR_1
ISR(USART_TX_vect_1)
{
    Board::detail::UART::indicateTxComplete(1);
}
#endif

///

#endif
