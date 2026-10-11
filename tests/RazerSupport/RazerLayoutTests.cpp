/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <iostream>
#include <stdexcept>
#include <cstring>
#include "RazerDevices.h"
static void Check(bool pass, const char* label)
{
    if(!pass) throw std::runtime_error(label);
    std::cout << "PASS " << label << '\n';
}
int main()
{
    try
    {
        const unsigned short pids[] = {RAZER_BLACKWIDOW_V3_MINI_WIRED_PID, RAZER_BLACKWIDOW_V3_MINI_WIRELESS_PID};
        const KEYBOARD_LAYOUT layouts[] = {KEYBOARD_LAYOUT_ANSI_QWERTY, KEYBOARD_LAYOUT_ISO_QWERTY,
                                         KEYBOARD_LAYOUT_ISO_AZERTY, KEYBOARD_LAYOUT_ISO_QWERTZ};
        for(unsigned short pid : pids)
        {
            const razer_device* device = nullptr;
            for(unsigned int i = 0; i < RAZER_NUM_DEVICES; i++) if(device_list[i]->pid == pid) device = device_list[i];
            Check(device && device->rows == 5 && device->cols == 16, "existing hardware address space remains 5 x 16");
            for(KEYBOARD_LAYOUT layout : layouts)
            {
                KeyboardLayoutManager fixed(layout,device->layout->base_size,device->layout->key_values);
                fixed.ChangeKeys(*device->layout);
                keyboard_keymap_overlay_values old = *device->layout;
                for(keyboard_led& key : old.edit_keys)
                    if(key.row == 4 && (key.col == 14 || key.col == 15)) key.opcode = KEYBOARD_OPCODE_SWAP_ONLY;
                KeyboardLayoutManager original(layout,old.base_size,old.key_values);
                original.ChangeKeys(old);
                matrix_map_type before = original.GetKeyMap(KEYBOARD_MAP_FILL_TYPE_INDEX,5,16);
                matrix_map_type after = fixed.GetKeyMap(KEYBOARD_MAP_FILL_TYPE_INDEX,5,16);
                Check(before.map[78] == 0xFFFFFFFF && before.map[79] == 0xFFFFFFFF, "reproduces missing terminal arrows in original overlay");
                Check(after.map[78] == 78 && after.map[79] == 79, "down/right retain existing row 4 column 14/15 hardware addresses");
                Check(fixed.GetKeyNameAt(4,14) == KEY_EN_DOWN_ARROW && fixed.GetKeyNameAt(4,15) == KEY_EN_RIGHT_ARROW,
                      "arrow names are available to the device view and SDK matrix");
                bool unchanged = true;
                for(unsigned int i = 0; i < 78; i++)
                    unchanged &= before.map[i] == after.map[i]
                              && original.GetKeyNameAt(i/16,i%16) == fixed.GetKeyNameAt(i/16,i%16);
                Check(unchanged, "all other key names and matrix indices remain unchanged");
            }
        }
        for(unsigned short pid : {RAZER_BASILISK_V3_PRO_35K_PG_WIRED_PID,
                                  RAZER_BASILISK_V3_PRO_35K_PG_WIRELESS_PID})
        {
            const razer_device* device = nullptr;
            for(unsigned int i = 0; i < RAZER_NUM_DEVICES; i++) if(device_list[i]->pid == pid) device = device_list[i];
            unsigned int leds = 0;
            for(unsigned int z = 0; z < 6; z++) if(device->zones[z]) leds += device->zones[z]->rows * device->zones[z]->cols;
            Check(device->rows * device->cols == 12 && leds == 12,
                  "Phantom Green frame has exactly 12 colors and no logo slot or out-of-bounds read");
        }
        return 0;
    }
    catch(const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
