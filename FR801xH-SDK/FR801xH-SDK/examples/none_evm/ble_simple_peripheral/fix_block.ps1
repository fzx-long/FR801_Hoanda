 = 'code/ble_function.c' 
 = Get-Content  -Raw 
 = '(?s)        if \(payload_len  && payload != NULL\)\{.*?return 0u;' 
 = @' 
        if (payload == NULL) { 
            return 0u; 
        } 
        const bool has_time_prefix = (payload_len 
        bool has_control = false; 
        bool has_control_type = false; 
        uint8_t control = 0u; 
        uint8_t controlType = 0u; 
        if (has_time_prefix) { 
            control = payload[6]; 
            controlType = payload[7]; 
            has_control = true; 
            has_control_type = true; 
        } else { 
            if (payload_len  { 
                control = payload[0]; 
                has_control = true; 
            } 
            if (payload_len  { 
                controlType = payload[1]; 
                has_control_type = true; 
            } 
        } 
        if (has_control && has_control_type && controlType == 0x04u) { 
            return (control == 0x01u) ? (uint16_t)CMD_CHARGE_DISPLAY_ON 
                                      : (uint16_t)CMD_CHARGE_DISPLAY_OFF; 
        } 
        if (has_control && has_control_type && controlType == 0x0Au) { 
            if (control == 0x01u) 
                return (uint16_t)CMD_Three_shooting_lamp_on; 
            if (control == 0x00u) 
                return (uint16_t)CMD_Three_shooting_lamp_off; 
            if (control == 0x02u) 
                return (uint16_t)CMD_Three_shooting_lamp_default; 
            return 0u; 
        } 
        if (has_control) { 
            if (control == 0x01u) 
