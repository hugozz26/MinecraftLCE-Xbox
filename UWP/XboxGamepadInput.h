// ============================================================================
// XboxGamepadInput.h - Xbox One Gamepad Input via Windows.Gaming.Input
// ============================================================================
// Replaces XInput9_1_0 with UWP-compatible Windows.Gaming.Input API
// Maps Xbox controller buttons to the game's internal input system
// ============================================================================

#pragma once

#include <Windows.Gaming.Input.h>
#include <collection.h>
#include <vector>
#include <mutex>

using namespace Windows::Gaming::Input;

// Button mapping indices matching the game's internal button IDs
// (from 4J_Input definitions)
enum class GameButton : int
{
    A = 0,
    B = 1,
    X = 2,
    Y = 3,
    LB = 4,
    RB = 5,
    LT = 6,
    RT = 7,
    Back = 8,
    Start = 9,
    LeftStick = 10,
    RightStick = 11,
    DPadUp = 12,
    DPadDown = 13,
    DPadLeft = 14,
    DPadRight = 15,
    Count = 16
};

struct GamepadState
{
    bool buttons[static_cast<int>(GameButton::Count)];
    bool prevButtons[static_cast<int>(GameButton::Count)];
    float leftStickX;
    float leftStickY;
    float rightStickX;
    float rightStickY;
    float leftTrigger;
    float rightTrigger;
    bool connected;
};

class XboxGamepadInput
{
public:
    static const int MAX_GAMEPADS = 4;

    XboxGamepadInput()
    {
        for (int i = 0; i < MAX_GAMEPADS; i++)
        {
            ZeroMemory(&m_states[i], sizeof(GamepadState));
        }

        // Register for gamepad added/removed events
        Gamepad::GamepadAdded +=
            ref new Windows::Foundation::EventHandler<Gamepad^>(
                [this](Platform::Object^, Gamepad^ pad) { OnGamepadAdded(pad); });

        Gamepad::GamepadRemoved +=
            ref new Windows::Foundation::EventHandler<Gamepad^>(
                [this](Platform::Object^, Gamepad^ pad) { OnGamepadRemoved(pad); });

        // Enumerate already-connected gamepads
        auto gamepads = Gamepad::Gamepads;
        for (unsigned int i = 0; i < gamepads->Size && i < MAX_GAMEPADS; i++)
        {
            m_gamepads.push_back(gamepads->GetAt(i));
        }
    }

    ~XboxGamepadInput() {}

    void Update()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (int i = 0; i < MAX_GAMEPADS && i < static_cast<int>(m_gamepads.size()); i++)
        {
            // Save previous state
            memcpy(m_states[i].prevButtons, m_states[i].buttons, sizeof(m_states[i].buttons));

            // Read current state
            GamepadReading reading = m_gamepads[i]->GetCurrentReading();

            m_states[i].connected = true;

            // Digital buttons
            m_states[i].buttons[static_cast<int>(GameButton::A)] =
                (reading.Buttons & GamepadButtons::A) == GamepadButtons::A;
            m_states[i].buttons[static_cast<int>(GameButton::B)] =
                (reading.Buttons & GamepadButtons::B) == GamepadButtons::B;
            m_states[i].buttons[static_cast<int>(GameButton::X)] =
                (reading.Buttons & GamepadButtons::X) == GamepadButtons::X;
            m_states[i].buttons[static_cast<int>(GameButton::Y)] =
                (reading.Buttons & GamepadButtons::Y) == GamepadButtons::Y;
            m_states[i].buttons[static_cast<int>(GameButton::LB)] =
                (reading.Buttons & GamepadButtons::LeftShoulder) == GamepadButtons::LeftShoulder;
            m_states[i].buttons[static_cast<int>(GameButton::RB)] =
                (reading.Buttons & GamepadButtons::RightShoulder) == GamepadButtons::RightShoulder;
            m_states[i].buttons[static_cast<int>(GameButton::Back)] =
                (reading.Buttons & GamepadButtons::View) == GamepadButtons::View;
            m_states[i].buttons[static_cast<int>(GameButton::Start)] =
                (reading.Buttons & GamepadButtons::Menu) == GamepadButtons::Menu;
            m_states[i].buttons[static_cast<int>(GameButton::LeftStick)] =
                (reading.Buttons & GamepadButtons::LeftThumbstick) == GamepadButtons::LeftThumbstick;
            m_states[i].buttons[static_cast<int>(GameButton::RightStick)] =
                (reading.Buttons & GamepadButtons::RightThumbstick) == GamepadButtons::RightThumbstick;
            m_states[i].buttons[static_cast<int>(GameButton::DPadUp)] =
                (reading.Buttons & GamepadButtons::DPadUp) == GamepadButtons::DPadUp;
            m_states[i].buttons[static_cast<int>(GameButton::DPadDown)] =
                (reading.Buttons & GamepadButtons::DPadDown) == GamepadButtons::DPadDown;
            m_states[i].buttons[static_cast<int>(GameButton::DPadLeft)] =
                (reading.Buttons & GamepadButtons::DPadLeft) == GamepadButtons::DPadLeft;
            m_states[i].buttons[static_cast<int>(GameButton::DPadRight)] =
                (reading.Buttons & GamepadButtons::DPadRight) == GamepadButtons::DPadRight;

            // Analog sticks
            m_states[i].leftStickX = static_cast<float>(reading.LeftThumbstickX);
            m_states[i].leftStickY = static_cast<float>(reading.LeftThumbstickY);
            m_states[i].rightStickX = static_cast<float>(reading.RightThumbstickX);
            m_states[i].rightStickY = static_cast<float>(reading.RightThumbstickY);

            // Triggers (0.0 - 1.0)
            m_states[i].leftTrigger = static_cast<float>(reading.LeftTrigger);
            m_states[i].rightTrigger = static_cast<float>(reading.RightTrigger);

            // Map triggers to digital buttons
            m_states[i].buttons[static_cast<int>(GameButton::LT)] = (m_states[i].leftTrigger > 0.5f);
            m_states[i].buttons[static_cast<int>(GameButton::RT)] = (m_states[i].rightTrigger > 0.5f);
        }

        // Mark disconnected pads
        for (int i = static_cast<int>(m_gamepads.size()); i < MAX_GAMEPADS; i++)
        {
            m_states[i].connected = false;
        }
    }

    const GamepadState& GetState(int padIndex) const
    {
        assert(padIndex >= 0 && padIndex < MAX_GAMEPADS);
        return m_states[padIndex];
    }

    bool IsButtonDown(int padIndex, GameButton button) const
    {
        if (padIndex < 0 || padIndex >= MAX_GAMEPADS) return false;
        return m_states[padIndex].buttons[static_cast<int>(button)];
    }

    bool IsButtonPressed(int padIndex, GameButton button) const
    {
        if (padIndex < 0 || padIndex >= MAX_GAMEPADS) return false;
        int idx = static_cast<int>(button);
        return m_states[padIndex].buttons[idx] && !m_states[padIndex].prevButtons[idx];
    }

    bool IsButtonReleased(int padIndex, GameButton button) const
    {
        if (padIndex < 0 || padIndex >= MAX_GAMEPADS) return false;
        int idx = static_cast<int>(button);
        return !m_states[padIndex].buttons[idx] && m_states[padIndex].prevButtons[idx];
    }

    int GetConnectedCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<int>(m_gamepads.size());
    }

    bool IsConnected(int padIndex) const
    {
        return m_states[padIndex].connected;
    }

private:
    void OnGamepadAdded(Gamepad^ pad)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_gamepads.size() < MAX_GAMEPADS)
        {
            m_gamepads.push_back(pad);
        }
    }

    void OnGamepadRemoved(Gamepad^ pad)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_gamepads.begin(); it != m_gamepads.end(); ++it)
        {
            if (*it == pad)
            {
                m_gamepads.erase(it);
                break;
            }
        }
    }

    std::vector<Gamepad^> m_gamepads;
    GamepadState m_states[MAX_GAMEPADS];
    mutable std::mutex m_mutex;
};

// Global instance
extern XboxGamepadInput g_XboxGamepad;
