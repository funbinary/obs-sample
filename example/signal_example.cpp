#include <iostream>

#include "callback/signal.h"

using namespace std;

static const char* obs_signals[] = {
    "void source_create(ptr source)",
    "void source_destroy(ptr source)",
    "void source_remove(ptr source)",
    "void source_update(ptr source)",
    "void source_save(ptr source)",
    "void source_load(ptr source)",
    "void source_activate(ptr source)",
    "void source_deactivate(ptr source)",
    "void source_show(ptr source)",
    "void source_hide(ptr source)",
    "void source_audio_activate(ptr source)",
    "void source_audio_deactivate(ptr source)",
    "void source_rename(ptr source, string new_name, string prev_name)",
    "void source_volume(ptr source, in out float volume)",
    "void source_volume_level(ptr source, float level, float magnitude, "
    "float peak)",
    "void source_transition_start(ptr source)",
    "void source_transition_video_stop(ptr source)",
    "void source_transition_stop(ptr source)",
    "void channel_change(int channel, in out ptr source, ptr prev_source)",
    "void hotkey_layout_change()",
    "void hotkey_register(ptr hotkey)",
    "void hotkey_unregister(ptr hotkey)",
    "void hotkey_bindings_changed(ptr hotkey)",
    NULL,
};

void SourceCreatedMultiHandler(void* param, calldata_t* data)
{
    cout << " call  " << endl;
}

int main()
{
    signal_handler_t* signals = signal_handler_create();

    signal_handler_add_array(signals, obs_signals);
    signal_handler_connect(signals, "source_create", SourceCreatedMultiHandler, nullptr);

    struct calldata params = {0};

    signal_handler_signal(signals, "source_create", &params);
    signal_handler_destroy(signals);
}
