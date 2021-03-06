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

#include "Touchscreen.h"

using namespace IO;

#define MODEL _modelPtr[_activeModel]

uint8_t IO::Touchscreen::Model::Common::rxBuffer[IO::Touchscreen::Model::Common::bufferSize];
size_t  IO::Touchscreen::Model::Common::bufferCount;

bool Touchscreen::init()
{
    if (_database.read(Database::Section::touchscreen_t::setting, static_cast<size_t>(IO::Touchscreen::setting_t::enable)))
    {
        auto dbModel = static_cast<IO::Touchscreen::Model::model_t>(_database.read(Database::Section::touchscreen_t::setting, static_cast<size_t>(IO::Touchscreen::setting_t::model)));

        if (_initialized)
        {
            if (static_cast<uint8_t>(dbModel) == _activeModel)
                return true;    //nothing to do, same model already _initialized

            if (!deInit())
                return false;
        }

        if (!isModelValid(dbModel))
            return false;

        _activeModel = static_cast<uint8_t>(dbModel);
        _initialized = MODEL->init();

        if (_initialized)
        {
            setScreen(_database.read(Database::Section::touchscreen_t::setting, static_cast<size_t>(IO::Touchscreen::setting_t::initialScreen)));
            setBrightness(static_cast<brightness_t>(_database.read(Database::Section::touchscreen_t::setting, static_cast<size_t>(IO::Touchscreen::setting_t::brightness))));

            return true;
        }
    }

    return false;
}

bool Touchscreen::deInit()
{
    if (!_initialized)
        return false;    //nothing to do

    if (MODEL->deInit())
    {
        _initialized = false;
        _activeModel = static_cast<uint8_t>(Model::model_t::AMOUNT);
        return true;
    }

    return false;
}

void Touchscreen::update()
{
    if (!_initialized)
        return;

    tsData_t  tsData;
    tsEvent_t event = MODEL->update(tsData);

    switch (event)
    {
    case tsEvent_t::button:
        processButton(tsData.buttonID, tsData.buttonState);
        break;

    case tsEvent_t::coordinate:
        processCoordinate(tsData.pressType, tsData.xPos, tsData.yPos);
        break;

    default:
        break;
    }
}

/// Switches to requested screen on display
/// param [in]: screenID  Index of screen to display.
void Touchscreen::setScreen(size_t screenID)
{
    if (!_initialized)
        return;

    MODEL->setScreen(screenID);
    _activeScreenID = screenID;

    if (_eventNotifier != nullptr)
        _eventNotifier->screenChange(screenID);
}

/// Used to retrieve currently active screen on display.
/// returns: Active display screen.
size_t Touchscreen::activeScreen()
{
    return _activeScreenID;
}

void Touchscreen::registerEventNotifier(EventNotifier& eventNotifer)
{
    _eventNotifier = &eventNotifer;
}

void Touchscreen::setIconState(size_t index, bool state)
{
    if (!_initialized)
        return;

    if (index >= MAX_NUMBER_OF_TOUCHSCREEN_COMPONENTS)
        return;

    icon_t icon;

    icon.onScreen  = _database.read(Database::Section::touchscreen_t::onScreen, index);
    icon.offScreen = _database.read(Database::Section::touchscreen_t::offScreen, index);

    if (icon.onScreen == icon.offScreen)
        return;    //invalid screen indexes

    if ((_activeScreenID != icon.onScreen) && (_activeScreenID != icon.offScreen))
        return;    //don't allow setting icon on wrong screen

    icon.xPos   = _database.read(Database::Section::touchscreen_t::xPos, index);
    icon.yPos   = _database.read(Database::Section::touchscreen_t::yPos, index);
    icon.width  = _database.read(Database::Section::touchscreen_t::width, index);
    icon.height = _database.read(Database::Section::touchscreen_t::height, index);

    MODEL->setIconState(icon, state);
}

bool Touchscreen::isModelValid(Model::model_t model)
{
    if (model == IO::Touchscreen::Model::model_t::AMOUNT)
        return false;

    if (_modelPtr[static_cast<uint8_t>(model)] == nullptr)
        return false;

    return true;
}

bool Touchscreen::registerModel(IO::Touchscreen::Model::model_t model, Model* ptr)
{
    if (model == IO::Touchscreen::Model::model_t::AMOUNT)
        return false;

    _modelPtr[static_cast<uint8_t>(model)] = ptr;
    return true;
}

void Touchscreen::processButton(const size_t buttonID, const bool state)
{
    bool   changeScreen = false;
    size_t newScreen    = 0;

    if (_database.read(Database::Section::touchscreen_t::pageSwitchEnabled, buttonID))
    {
        changeScreen = true;
        newScreen    = _database.read(Database::Section::touchscreen_t::pageSwitchIndex, buttonID);
    }

    _cInfo.send(Database::block_t::touchscreen, buttonID);

    if (_eventNotifier != nullptr)
        _eventNotifier->button(buttonID, state);

    //if the button should change screen, change it immediately
    //this will result in button never sending off state so do it manually first
    if (changeScreen)
    {
        if (_eventNotifier != nullptr)
            _eventNotifier->button(buttonID, false);

        setScreen(newScreen);
    }
}

bool Touchscreen::setBrightness(brightness_t brightness)
{
    if (!_initialized)
        return false;

    return MODEL->setBrightness(brightness);
}

void Touchscreen::processCoordinate(pressType_t pressType, uint16_t xPos, uint16_t yPos)
{
    for (size_t i = 0; i < MAX_NUMBER_OF_TOUCHSCREEN_COMPONENTS; i++)
    {
        if (_database.read(Database::Section::touchscreen_t::analogPage, i) == static_cast<int32_t>(activeScreen()))
        {
            uint16_t startXCoordinate = _database.read(Database::Section::touchscreen_t::analogStartXCoordinate, i);
            uint16_t endXCoordinate   = _database.read(Database::Section::touchscreen_t::analogEndXCoordinate, i);
            uint16_t startYCoordinate = _database.read(Database::Section::touchscreen_t::analogStartYCoordinate, i);
            uint16_t endYCoordinate   = _database.read(Database::Section::touchscreen_t::analogEndYCoordinate, i);

            uint16_t startCoordinate;
            uint16_t endCoordinate;
            uint16_t value;

            //x
            if (_database.read(Database::Section::touchscreen_t::analogType, i) == static_cast<int32_t>(IO::Touchscreen::analogType_t::horizontal))
            {
                value = xPos;

                if (pressType == IO::Touchscreen::pressType_t::hold)
                {
                    //y coordinate can be ignored once the touchscreen is pressed, verify and constrain x range only
                    if (value > endXCoordinate)
                        value = endXCoordinate;
                    else if (value < startXCoordinate)
                        value = startXCoordinate;
                }
                else if (pressType == IO::Touchscreen::pressType_t::initial)
                {
                    if (((value < startXCoordinate) || (value > endXCoordinate)) || ((yPos < startYCoordinate) || (yPos > endYCoordinate)))
                        continue;
                    else
                        _analogActive[i] = true;
                }
                else
                {
                    _analogActive[i] = false;
                }

                startCoordinate = startXCoordinate;
                endCoordinate   = endXCoordinate;
            }
            //y
            else
            {
                value = yPos;

                if (pressType == IO::Touchscreen::pressType_t::hold)
                {
                    //x coordinate can be ignored once the touchscreen is pressed, verify and constrain x range only
                    if (value > endYCoordinate)
                        value = endYCoordinate;
                    else if (value < startYCoordinate)
                        value = startYCoordinate;
                }
                else if (pressType == IO::Touchscreen::pressType_t::initial)
                {
                    if (((xPos < startXCoordinate) || (xPos > endXCoordinate)) || ((value < startYCoordinate) || (value > endYCoordinate)))
                        continue;
                    else
                        _analogActive[i] = true;
                }
                else
                {
                    _analogActive[i] = false;
                }

                startCoordinate = startYCoordinate;
                endCoordinate   = endYCoordinate;
            }

            //scale the value to ADC range
            if (_analogActive[i])
            {
                if (_eventNotifier != nullptr)
                    _eventNotifier->analog(i, value, startCoordinate, endCoordinate);
            }
            else
            {
                if (_database.read(Database::Section::touchscreen_t::analogResetOnRelease, i))
                {
                    if (_eventNotifier != nullptr)
                        _eventNotifier->analog(i, 0, startCoordinate, endCoordinate);
                }
            }
        }
    }
}

bool Touchscreen::isInitialized() const
{
    return _initialized;
}