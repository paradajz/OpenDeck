/*

Copyright 2015-2020 Igor Petrovic

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

#include "board/Board.h"
#include "board/Internal.h"
#include "core/src/general/Timing.h"

#ifdef FW_APP
#ifdef ANALOG_SUPPORTED
///
/// \brief ADC ISR used to read values from multiplexers.
///
ISR(ADC_vect)
{
    Board::detail::isrHandling::adc(ADC);
}
#endif
#endif

///
/// \brief Main interrupt service routine.
/// Used to control I/O on board and to update current run time.
///
ISR(TIMER0_COMPA_vect)
{
    static bool _1ms = true;

    _1ms = !_1ms;

    if (_1ms)
    {
        core::timing::detail::rTime_ms++;

#ifdef FW_APP
#ifndef USB_LINK_MCU
#if MAX_NUMBER_OF_LEDS > 0
        Board::detail::io::checkDigitalOutputs();
#endif
#endif
#ifdef LED_INDICATORS
        Board::detail::io::checkIndicators();
#endif
#endif
    }

#ifdef FW_APP
#ifndef USB_LINK_MCU
    Board::detail::io::checkDigitalInputs();
#endif
#endif
}
